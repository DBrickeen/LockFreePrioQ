#include <iostream>
#include <math.h>
#include <atomic>
using namespace std;

// may need a compiler option that enforces the proper x86 cmpxch for DCAS

template<typename T, T maxValue> class LockFreeMound {
private:
    const unsigned int MAX_DEPTH = 16;
    const unsigned int THRESHOLD = 8;

    //list node
    struct LNode {
        T value;      //value stored in node
        LNode* next;  //next element in list
    };

    //concurrent mound node
    struct CMNode {
        LNode* list;  //sorted list of values stored at this node
        bool dirty;   //true if mound property does not hold
        int c;        //counter - incremented on every update
    };

    //a true mound node, storing two pointers for DCAS
    struct RealNode {
        CMNode *node;   // our current mount node
        RealNode *par;  // the parent of this node
    };

    atomic<RealNode> tree[MAX_DEPTH][];
    atomic<int> depth;

public:
    LockFreeMound() {
        //initialize tree
        //each second level array represents a level of the tree
        for (int i = 0; i < MAX_DEPTH; ++i) {
            tree[i] = RealNode[1<<i];
        }

        tree[0][0] = RealNode { new CMNode { NULL, false, 0 }, NULL };
        depth = 1;
    }

    T value(RealNode n) {
        return n.node->list ? n.node->list->value : maxValue;
    }

    void insert(T value) {
        while (true) {
            int n = findInsertPoint(value);
            int x = indexX(n), y = indexY(n, x);
            RealNode C = READ(tree[x][y]);
            if (value(C) >= value) {
                RealNode* P = NULL;
                if (n != 1) {
                    int p = parentIndex(n);
                    int px = parentIndexX(p), py = parentIndexY(p, px);
                    P = &(READ(tree[px][py]));
                }
                RealNode newNode = RealNode { new CMNode { new LNode { v, C.node->list }, C.node->dirty, C.node->c },
                                              P };
                if (n == 1) {
                    if (CAS(tree[x][y], &C, newNode))
                        return;
                    else
                        if (value(P) <= value)
                            if (CAS(tree[x][y], &C, newNode))
                                return;
                    delete(newNode.node->list);
                }
            }
        }
    }

    int findInsertPoint(T v) {
        while (true) {
            d = READ(depth);
            for (int i = 0; i < THRESHOLD; ++i) {
                int n = randLeaf(d);
                if (value(leaf) >= v)
                    return bs(leaf, 1, v);
            }
            depth.compare_exchange_weak(&d, d + 1);
        }
    }

    int randLeaf() {
        return (int) random.rand31_next() % (int) pow(2, depth - 1)
            + (int) pow(2, depth - 1) - 1;
    }

    T extractMin() {
        while (true) {
            RealNode R = READ(tree[0][0]);
            if (R.node->dirty) {
                moundify(1);
                continue;
            }
            if (R.node->list == NULL)
                return maxValue;
            if (CAS(tree[0][0], &R, RealNode { new CMNode { R.node->list->next, true, R.node->c + 1 }, R.par })) {
                T retval = R.node->list.value;
                delete(R.node->list);
                moundify(1);
                return retval;
            }
        }
    }

    void moundify(int n) {

    }

    //single integer position in tree
    int index(int x, int y) {
        return (1<<x) + y - 1;
    }

    //first index in tree
    int indexX(int z) {
        return log2(z + 1);
    }

    //second index in tree
    int indexY(int z, int x) {
        return z - (1<<x) + 1;
    }
    
    // I guess I should point out right now that this still doesn't behave as desired
    // It will update an atomic struct for one node correctly and the pointer for its parent
    // but the actual parent node will remain unchanged
    // we could propogate the change up but that is more runtime and may introduce weird race conditions
    // basically the problem with all the DCAS implementations i find online is that they are performing it
    // on a struct containing two pointers-sized variables, but one of them is just a small int
    // i haven't yet found an implementation where they are actually modifying two separate memory locations at once
    // which is what we really want to do here - we want to modify A[n] and A[n/2] with a DCAS
    // i might know a solution to this where we redefine the array of nodes to be a real tree instead of array-based
    // so when we modify a node, we actually do the DCAS properly
    // problem is, we lose the easy access of the array
    // it's certainly a thinker of a problem, i'll see if i can't figure it out
    
    // according to everything i've read, a pure DCAS function on two separate memory locations does not exist right now
    // DWCAS exists, but that only works with tag pointers associated with a pointer.  it's not what i want, which is CAS
    // on two separate memory locations at the same time
    // all the things we've looked at (including what you linked me) are DWCAS, which isn't helpful for this data structure
    // so the good news is, you can create a pseudo-DCAS through software to get the same results (though not the same runtime)
    // this is explained in the paper cited by our mound paper, so it's apparently how they want this to be done
    // since a pure DCAS is not hardware supported.
    // i will try to get this implemented
    bool CAS(atomic<RealNode> tree, RealNode* C, RealNode CP) {
        return tree.compare_exchange_weak(C, CP);
    }

    RealNode READ(atomic<RealNode> n) {
        return n;
    }

    int READ(atomic<int> n) {
        return n;
    }

};
