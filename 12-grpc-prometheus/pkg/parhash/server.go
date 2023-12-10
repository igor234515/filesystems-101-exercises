package parhash

import (
	"context"
	"log"
	"net"
	"sync"
	"time"

	"github.com/pkg/errors"
	"github.com/prometheus/client_golang/prometheus"
	"golang.org/x/sync/semaphore"
	"google.golang.org/grpc"

	hashpb "fs101ex/pkg/gen/hashsvc"
	parhashpb "fs101ex/pkg/gen/parhashsvc"
	"fs101ex/pkg/workgroup"
)

type Config struct {
	ListenAddr   string
	BackendAddrs []string
	Concurrency  int

	Prom prometheus.Registerer
}

// Implement a server that responds to ParallelHash()
// as declared in /proto/parhash.proto.
//
// The implementation of ParallelHash() must not hash the content
// of buffers on its own. Instead, it must send buffers to backends
// to compute hashes. Buffers must be fanned out to backends in the
// round-robin fashion.
//
// For example, suppose that 2 backends are configured and ParallelHash()
// is called to compute hashes of 5 buffers. In this case it may assign
// buffers to backends in this way:
//
//	backend 0: buffers 0, 2, and 4,
//	backend 1: buffers 1 and 3.
//
// Requests to hash individual buffers must be issued concurrently.
// Goroutines that issue them must run within /pkg/workgroup/Wg. The
// concurrency within workgroups must be limited by Server.sem.
//
// WARNING: requests to ParallelHash() may be concurrent, too.
// Make sure that the round-robin fanout works in that case too,
// and evenly distributes the load across backends.
//
// The server must report the following performance counters to Prometheus:
//
//  1. nr_nr_requests: a counter that is incremented every time a call
//     is made to ParallelHash(),
//
//  2. subquery_durations: a histogram that tracks durations of calls
//     to backends.
//     It must have a label `backend`.
//     Each subquery_durations{backed=backend_addr} must be a histogram
//     with 24 exponentially growing buckets ranging from 0.1ms to 10s.
//
// Both performance counters must be placed to Prometheus namespace "parhash".
type Server struct {
	config Config

	listener   net.Listener
	work_group sync.WaitGroup
	cancel     context.CancelFunc

	profiler  *Profiler
	semaphore *semaphore.Weighted

	mutex      sync.Mutex
	backend_ID int
}

type Profiler struct {
	nr_nr_requests     prometheus.Counter
	subquery_durations *prometheus.HistogramVec
}

func NewProfiler(registerer prometheus.Registerer) *Profiler {

	profiler := &Profiler{
		nr_nr_requests: prometheus.NewCounter(prometheus.CounterOpts{
			Namespace: "parhash",
			Name:      "nr_requests",
			Help:      "Call ParalellHash counter",
		}),
		subquery_durations: prometheus.NewHistogramVec(prometheus.HistogramOpts{
			Namespace: "parhash",
			Name:      "subquery_durations",
			Buckets:   prometheus.ExponentialBuckets(0.1, 1e4, 24),
			Help:      "Duration Histogram",
		},

			[]string{"backend"}),
	}

	registerer.MustRegister(profiler.subquery_durations)
	registerer.MustRegister(profiler.nr_nr_requests)
	return profiler
}

func New(conf Config) *Server {
	return &Server{
		semaphore: semaphore.NewWeighted(int64(conf.Concurrency)),
		config:    conf,
	}
}

func (s *Server) Start(ctx context.Context) (status error) {
	defer func() { status = errors.Wrap(status, "Start()") }()

	/* implement me */

	ctx, s.cancel = context.WithCancel(ctx)

	s.listener, status = net.Listen("tcp", s.config.ListenAddr)

	if status != nil {
		return status
	}

	server := grpc.NewServer()
	parhashpb.RegisterParallelHashSvcServer(server, s)

	s.work_group.Add(2)

	go func() {
		defer s.work_group.Done()

		server.Serve(s.listener)
	}()

	go func() {
		defer s.work_group.Done()

		<-ctx.Done()
		s.listener.Close()
	}()

	return nil
}

func (s *Server) ListenAddr() string {
	/* implement me */
	return s.listener.Addr().String()
}

func (s *Server) Stop() {
	/* implement me */
	s.cancel()
	s.work_group.Wait()
}

func (s *Server) ParallelHash(ctx context.Context, req *parhashpb.ParHashReq) (responce *parhashpb.ParHashResp, status error) {
	s.profiler.nr_nr_requests.Inc()
	var (
		backends_number = len(s.config.BackendAddrs)
		connections     = make([]*grpc.ClientConn, backends_number)
		clients         = make([]hashpb.HashSvcClient, backends_number)
		work_group      = workgroup.New(workgroup.Config{Sem: s.semaphore})
		hashes          = make([][]byte, len(req.Data))
	)

	for i := range clients {

		connections[i], status = grpc.Dial(s.config.BackendAddrs[i],
			grpc.WithInsecure())

		if status != nil {
			log.Fatalf("failed to connect to %q: %v", s.config.BackendAddrs[i], status)
		}

		defer connections[i].Close()

		clients[i] = hashpb.NewHashSvcClient(connections[i])
	}

	for i := range req.Data {
		hash_ind := i

		work_group.Go(ctx, func(ctx context.Context) (status error) {

			s.mutex.Lock()
			backend_ind := s.backend_ID
			s.backend_ID = (s.backend_ID + 1) % backends_number
			s.mutex.Unlock()

			start_time := time.Now()
			responce, status := clients[backend_ind].Hash(ctx, &hashpb.HashReq{Data: req.Data[hash_ind]})
			end_time := time.Since(start_time)

			s.profiler.subquery_durations.With(prometheus.Labels{"backend": s.config.BackendAddrs[backend_ind]}).Observe(float64(end_time.Microseconds()))

			if status != nil {
				return status
			}

			s.mutex.Lock()
			hashes[hash_ind] = responce.Hash
			s.mutex.Unlock()

			return nil
		})
	}

	if status := work_group.Wait(); status != nil {
		log.Fatalf("failed to hash files: %v", status)
	}

	return &parhashpb.ParHashResp{Hashes: hashes}, nil
}
