#include <solution.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

struct node
{
    bool is_leaf;
    int min_degree;     // minimum degree
    int key_number;     // current number of keys
    int *keys;          // keys
    struct node **kids; // child pointers
};

struct node *node_alloc_ini(bool leaf, uint L)
{
    struct node *root = (struct node *)calloc(1, sizeof(struct node));
    root->is_leaf = leaf;
    root->min_degree = L;
    root->key_number = 0;
    root->keys = (int *)calloc((2 * L), sizeof(int));
    root->kids = (struct node **)calloc((2 * L + 1), sizeof(struct node *));
    return root;
}
static void node_delete(struct node *tree)
{
    free(tree->kids);
    free(tree->keys);
    free(tree);
}

void node_free(struct node *tree)
{
    if (tree == NULL)
        return;
    if (!tree->is_leaf)
    {
        for (long int i = 0; i < tree->key_number + 1; ++i)
        {
            node_free(tree->kids[i]);
        }
    }
    free(tree->kids);
    free(tree->keys);
    free(tree);
}

struct btree
{
    int min_degree;    // minimum degree
    struct node *root; // pointer to root node
};

struct btree *btree_alloc(unsigned int L)
{
    struct btree *tree = (struct btree *)calloc(1, sizeof(struct btree));
    tree->root = NULL;
    tree->min_degree = L;
    return tree;
}

void btree_free(struct btree *t)
{
    node_free(t->root);
    free(t);
}

static void btree_split_child(struct node *spliting_node, int i)
{
    int min_degree = spliting_node->min_degree;
    struct node *splt_kid = spliting_node->kids[i];
    struct node *addl_node = node_alloc_ini(splt_kid->is_leaf, min_degree);
    addl_node->key_number = min_degree - 1;
    int j;
    for (j = 0; j < min_degree - 1; j++)
    {
        addl_node->keys[j] = splt_kid->keys[j + min_degree];
    }
    if (splt_kid->is_leaf == 0)
    {
        for (j = 0; j < min_degree; j++)
        {
            addl_node->kids[j] = splt_kid->kids[j + min_degree];
        }
    }
    splt_kid->key_number = min_degree - 1;
    for (j = spliting_node->key_number; j >= i + 1; j--)
    {
        spliting_node->kids[j + 1] = spliting_node->kids[j];
    }
    spliting_node->kids[i + 1] = addl_node;
    for (j = spliting_node->key_number - 1; j >= i; j--)
    {
        spliting_node->keys[j + 1] = spliting_node->keys[j];
    }
    spliting_node->keys[i] = splt_kid->keys[min_degree - 1];
    spliting_node->key_number += 1;
}

static void btree_insert_with_empty_place(struct node *x, int k)
{
    int i = x->key_number - 1;
    if (x->is_leaf == 1)
    {
        while (i >= 0 && x->keys[i] > k)
        {
            x->keys[i + 1] = x->keys[i];
            --i;
        }
        x->keys[i + 1] = k;
        x->key_number += 1;
    }
    else
    {
        while (i >= 0 && x->keys[i] > k)
            i--;
        if (x->kids[i + 1]->key_number == 2 * x->min_degree - 1)
        {
            btree_split_child(x, i + 1);
            if (k > x->keys[i + 1])
                i++;
        }
        btree_insert_with_empty_place(x->kids[i + 1], k);
    }
}

void btree_insert(struct btree *t, int x)
{
    if (btree_contains(t, x))
        return;
    if (t->root == NULL)
    {
        // allocate memory for root
        t->root = node_alloc_ini(true, t->min_degree);
        t->root->keys[0] = x;
        t->root->key_number = 1;
        return;
    }
    int min_degree = t->min_degree;
    struct node *sroot = t->root;
    if ((long int)sroot->key_number == (long int)2 * min_degree - 1)
    {
        struct node *shn = node_alloc_ini(0, min_degree);
        shn->kids[0] = sroot;
        btree_split_child(shn, 0);
        int iter = 0;
        if (shn->keys[0] < x)
            iter++;
        btree_insert_with_empty_place(shn->kids[iter], x);
        t->root = shn;
    }
    else
    {
        btree_insert_with_empty_place(sroot, x);
    }
}

static void btree_keys_merge(struct node *node, int index)
{
    if (node == NULL)
    {
        return;
    }
    struct node *left_node = node->kids[index];
    struct node *right_node = node->kids[index + 1];

    left_node->keys[node->min_degree - 1] = node->keys[index];
    for (long int i = 0; i < right_node->min_degree - 1; i++)
    {
        left_node->keys[i + node->min_degree] = right_node->keys[i];
    }
    if (!left_node->is_leaf)
    {
        for (long int i = 0; i < right_node->min_degree; i++)
        {
            left_node->kids[i + node->min_degree] = right_node->kids[i];
        }
    }
    left_node->key_number = 2 * left_node->min_degree - 1;
    // delete x from node
    for (long int j = index; j < node->key_number - 1; j++)
    {
        node->keys[j] = node->keys[j + 1];
    }
    for (long int j = index + 1; j < node->key_number; j++)
    {
        node->kids[j] = node->kids[j + 1];
    }
    --node->key_number;
    node_delete(right_node);
}
static void btree_key_delete(struct node *node, int x)
{
    if (node == NULL)
    {
        return;
    }
    long int index = 0;
    while (index < node->key_number && x > node->keys[index])
    { // find the first value greater than keys
        index++;
    }
    if (index < node->key_number && x == node->keys[index])
    { // if the keys is found
        if (node->is_leaf)
        { // The node is a leaf node
            for (long int i = index; i < node->key_number - 1; ++i)
            {
                node->keys[i] = node->keys[i + 1];
            }
            --(node->key_number);
            return;
        }
        if (node->kids[index]->key_number >= node->min_degree)
        {
            struct node *current = node->kids[index];
            while (!current->is_leaf)
            {
                current = current->kids[current->key_number];
            }
            int val = current->keys[current->key_number - 1];
            node->keys[index] = val;
            btree_key_delete(node->kids[index], val);
        }
        else if (node->kids[index + 1]->key_number >= node->min_degree)
        {
            struct node *current = node->kids[index + 1];
            while (!current->is_leaf)
            {
                current = current->kids[0];
            }
            int val = current->keys[0];
            node->keys[index] = val;
            btree_key_delete(node->kids[index + 1], val);
        }
        else
        {
            btree_keys_merge(node, index);
            btree_key_delete(node->kids[index], x);
        }
    }
    else
    {
        if (node->is_leaf)
            return;
        // Ask the brother node to borrow, then add it to the child node, and then delete i
        if (node->kids[index]->key_number == node->min_degree - 1)
        {
            if ((index > 0 && node->kids[index - 1]->key_number >= node->min_degree) ||
                (index < node->key_number && node->kids[index + 1]->key_number >= node->min_degree))
            {
                if (index > 0 && node->kids[index - 1]->key_number >= node->min_degree)
                {
                    struct node *left_node = node->kids[index];
                    struct node *right_node = node->kids[index - 1];
                    for (int i = left_node->key_number - 1; i >= 0; --i)
                    {
                        left_node->keys[i + 1] = left_node->keys[i];
                    }
                    if (!left_node->is_leaf)
                    {
                        for (long int i = left_node->key_number; i >= 0; i--)
                        {
                            left_node->kids[i + 1] = left_node->kids[i];
                        }
                    }
                    ++left_node->key_number;
                    left_node->keys[0] = node->keys[index - 1];
                    if (!left_node->is_leaf)
                    {
                        left_node->kids[0] = right_node->kids[right_node->key_number];
                    }
                    node->keys[index - 1] = right_node->keys[right_node->key_number - 1];
                    --right_node->key_number;
                }
                else // if (idx < node->n && node->kids[idx + 1]->n >= T->t)
                {
                    struct node *left_node = node->kids[index];
                    struct node *right_node = node->kids[index + 1];
                    left_node->keys[left_node->key_number] = node->keys[index];
                    ++left_node->key_number;
                    if (!left_node->is_leaf)
                    {
                        left_node->kids[left_node->key_number] = right_node->kids[0];
                    }
                    node->keys[index] = right_node->keys[0];
                    for (long int i = 0; i < right_node->key_number - 1; ++i)
                    {
                        right_node->keys[i] = right_node->keys[i + 1];
                    }
                    if (!right_node->is_leaf)
                    {
                        for (long int i = 0; i < right_node->key_number + 1; ++i)
                        {
                            right_node->kids[i] = right_node->kids[i + 1];
                        }
                    }
                    --node->kids[index + 1]->key_number;
                }
            }
            else
            {
                if (index > 0)
                {
                    btree_keys_merge(node, index - 1);
                    --index;
                }
                else
                {
                    btree_keys_merge(node, index);
                }
            }
        }
        btree_key_delete(node->kids[index], x);
        if (node->key_number == 0)
        {
            node = node->kids[index];
        }
    }
}
void btree_delete(struct btree *t, int x)
{
    if (!btree_contains(t, x))
        return;
    if (!t->root)
        return;
    btree_key_delete(t->root, x);
    if (t->root->key_number == 0)
    {
        struct node *root = t->root;
        if (t->root->is_leaf)
            t->root = NULL;
        else
            t->root = t->root->kids[0];
        node_delete(root);
    }
}
bool node_contains(struct node *node, int x)
{
    if (!node)
        return false;
    long int i = 0;
    while (i < node->key_number && x > node->keys[i])
        ++i;
    if (i < node->key_number && node->keys[i] == x)
        return true;
    if (node->is_leaf)
        return false;
    return node_contains(node->kids[i], x);
}

bool btree_contains(struct btree *t, int x)
{
    return node_contains(t->root, x);
}
struct btree_iter;

int count(struct node *tree)
{
    if (!tree)
        return 0;
    int cnt = tree->key_number;
    if (tree->is_leaf)
    {
    }
    else
    {
        for (long int i = 0; i <= tree->key_number; i++)
        {
            cnt += count(tree->kids[i]);
        }
    }
    return cnt;
}

static void traverse_tree(struct node *node, int *this_node, int *index)
{
    if (!node)
        return;
    for (long int i = 0; i < node->key_number; ++i)
    {
        if (!node->is_leaf)
        {
            traverse_tree(node->kids[i], this_node, index);
        }
        this_node[*index] = node->keys[i];
        ++(*index);
    }
    if (!node->is_leaf)
    {
        traverse_tree(node->kids[node->key_number], this_node, index);
    }
}

struct btree_iter
{
    int *value_pointers;
    int counter;
    int ii;
};
struct btree_iter *btree_iter_start(struct btree *t)
{
    struct btree_iter *itering = (struct btree_iter *)calloc(1, sizeof(struct btree_iter));
    int counter = count(t->root);
    itering->counter = counter;
    itering->value_pointers = calloc(counter, sizeof(int));
    int index = 0;
    traverse_tree(t->root, itering->value_pointers, &index);
    return itering;
}

void btree_iter_end(struct btree_iter *i)
{
    free(i->value_pointers);
    free(i);
}

bool btree_iter_next(struct btree_iter *i, int *x)
{
    if (i->ii == i->counter)
        return false;
    *x = i->value_pointers[i->ii];
    ++i->ii;
    return true;
}