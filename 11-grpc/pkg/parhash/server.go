package parhash

import (
	"context"
	"fs101ex/pkg/workgroup"
	"log"
	"net"
	"sync"

	"github.com/pkg/errors"
	"golang.org/x/sync/semaphore"
	"google.golang.org/grpc"

	hashpb "fs101ex/pkg/gen/hashsvc"
	parhashpb "fs101ex/pkg/gen/parhashsvc"
)

type Config struct {
	ListenAddr   string
	BackendAddrs []string
	Concurrency  int
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
type Server struct {
	conf       Config
	ID         int
	listener   net.Listener
	work_group sync.WaitGroup
	mutex      sync.Mutex
	stop       context.CancelFunc
	sem        *semaphore.Weighted
}

func New(conf Config) *Server {
	return &Server{
		conf: conf,
		sem:  semaphore.NewWeighted(int64(conf.Concurrency)),
	}
}

func (s *Server) Start(ctx context.Context) (status error) {
	defer func() { status = errors.Wrap(status, "Start()") }()

	/* implement me */
	ctx, s.stop = context.WithCancel(ctx)

	s.listener, status = net.Listen("tcp", s.conf.ListenAddr)

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
	return s.listener.Addr().String()
}

func (s *Server) Stop() {
	s.stop()
	s.work_group.Wait()
}

func (s *Server) ParallelHash(ctx context.Context, req *parhashpb.ParHashReq) (resp *parhashpb.ParHashResp, status error) {

	var (
		backends_number = len(s.conf.BackendAddrs)
		clients         = make([]hashpb.HashSvcClient, backends_number)
		connections     = make([]*grpc.ClientConn, backends_number)
		hashes          = make([][]byte, len(req.Data))
		work_group      = workgroup.New(workgroup.Config{Sem: s.sem})
	)

	for i := range connections {
		connections[i], status = grpc.Dial(s.conf.BackendAddrs[i], grpc.WithInsecure() /* allow non-TLS connections */)

		if status != nil {
			log.Fatalf("Connection failed to %q: %v", s.conf.BackendAddrs[i], status)
		}

		defer connections[i].Close()
		clients[i] = hashpb.NewHashSvcClient(connections[i])
	}

	for i := range req.Data {
		var hs_num int = i
		work_group.Go(ctx, func(ctx context.Context) error {

			s.mutex.Lock()
			back_num := s.ID
			s.ID = (s.ID + 1) % backends_number
			s.mutex.Unlock()

			responce, status := clients[back_num].Hash(ctx, &hashpb.HashReq{Data: req.Data[hs_num]})
			if status != nil {
				return status
			}

			s.mutex.Lock()
			hashes[hs_num] = responce.Hash
			s.mutex.Unlock()

			return nil
		})
	}

	if status := work_group.Wait(); status != nil {
		log.Fatalf("Hashing failed: %v", status)
	}

	return &parhashpb.ParHashResp{Hashes: hashes}, nil
}
