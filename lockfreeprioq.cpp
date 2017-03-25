#include <iostream>
#include <math.h>
#include <atomic>
using namespace std;
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
    //also doubles as the DCSS and DCAS descriptors
    //because working with null pointers and trying to keep it safe was too much hassle
    struct CMNode {
        LNode* list;  //sorted list of values stored at this node
        bool dirty;   //true if mound property does not hold
        int c;        //counter - incremented on every update

        bool isDCSS;  // flag to determine if this is a DCSS descriptor
        bool isDCAS;  // flag to determine if this is a DCAS descriptor

        atomic<CMNode>* a1;
        CMNode *o1;
        CMNode *n1;
        atomic<CMNode>* a2;
        CMNode *o2;
        CMNode *n2;
        int count;
        atomic<CMNode>* status;
        int check;
        // 0 - UNDECIDED
        // 1 - FAILED
        // 2 - SUCCEEDED

        bool operator==(const CMNode& r) const {
            return list == r.list && dirt == r.dirty && c == r.c &&
                   isDCSS == r.isDCSS && isDCAS == r.isDCAS && a1 == r.a1 &&
                   o1 == r.o1 && n1 == r.n1 && a2 == r.a2 && o2 == r.o2 &&
                   n2 == r.n2 && count == r.count && status == r.status && check == r.check;
        }
    };

    atomic<CMNode> tree[MAX_DEPTH][];
    atomic<int> depth;

public:
    LockFreeMound() {
        //initialize tree
        //each second level array represents a level of the tree
        for (int i = 0; i < MAX_DEPTH; ++i) {
            tree[i] = CMNode[1<<i];
        }

        tree[0][0] = CMNode { NULL, false, 0, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0 };
        depth = 1;
    }

    T value(CMNode n) {
        return n->list ? n->list->value : maxValue;
    }

    void insert(T value) {
        while (true) {
            int n = findInsertPoint(value);
            int x = indexX(n), y = indexY(n, x);
            CMNode C = READ(tree[x][y]);
            if (value(C) >= value) {
                CMNode CP = CMNode { new LNode { v, C.list }, C.dirty, C.c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0 };
                if (n == 1) {
                    if (tree[x][y].compare_exchange_weak(&C, CP))
                        return;
                    else {
                        int p = parentIndex(n);
                        int px = indexX(p), py = indexY(p, px);
                        CMNode P = READ(tree[px][py]);
                        if (value(P) <= value)
                            if (DCSS(buildDCSS1(tree[x] + y, &C, &newNode, tree[px] + py, &P)))
                                return;
                    }
                    delete(CP.list);
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
    
    // note: this is not implemented yet
    // the concurrent queue's implementation is not a binary search, it is linear
    // so it won't get the proper runtime.
    int bs(int bottom, int top, int v) {
        
    }

    int randLeaf() {
        return (int) random.rand31_next() % (int) pow(2, depth - 1)
            + (int) pow(2, depth - 1) - 1;
    }

    T extractMin() {
        while (true) {
            CMNode R = READ(tree[0][0]);
            if (R.dirty) {
                moundify(1);
                continue;
            }
            if (R.list == NULL)
                return maxValue;
            if (tree[0][0].compare_exchange_weak(&R, CMNode {R.list->next, true, R.c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0})) {
                T retval = R.list->value;
                delete(R.list);
                moundify(1);
                return retval;
            }
        }
    }

    void moundify(int n) {
        while (true) {
            int x = indexX(n), y = indexY(n, x);
            CMNode N = READ(tree[x][y]);
            int d = READ(depth);
            if (!N.dirty)
                return;
            if (n >= (1<<(d-1)) - 1)
                return;
            int lx = indexX(2 * n + 1), ly = indexY(2 * n + 1, lx);
            CMNode L = READ(tree[lx][ly]);
            int rx = indexX(2 * n + 2), ry = indexY(2 * n + 2, rx);
            CMNode R = READ(tree[rx][ry]);
            if (L.dirty) {
                moundify(2 * n + 1);
                continue;
            }
            if (R.dirty) {
                moundify(2 * n + 2);
                continue;
            }
            if (value(L) <= value(R) && value(L) < value(N)) {
                if (DCAS(buildDCAS(tree[x] + y, &N, new CMNode {L.list, false, N.c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0 },
                                   tree[lx] + ly, &L, new CMNode {N.list, true, L.c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0 }))) {
                    moundify(2 * n + 1);
                    return;
                }
            }
            else if (value(R) < value(L) && value(R) < value(N)) {
                if (DCAS(buildDCAS(tree[x] + y, &N, new CMNode {R.list, false, N.c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0},
                                   tree[rx] + ry, &R, new CMNode {N.list, true, R.c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}))) {
                    moundify(2 * n + 2);
                    return;
                }
            }
            else if (tree[x][y].compare_exchange_weak(&N, CMNode {N.list, false, N.c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}))
                return;
        }
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

    // basing the DCSS and DCAS operations on the article:
    // "A Practical Multi-word Compare-and-Swap Operation"
    // by Harris, Fraser, and Pratt
    CMNode* CAS(atomic<CMNode>* tree, CMNode *C, CMNode CP) {
        if (tree->compare_exchange_weak(C, CP))
            return C;
        return C;
    }

    CMNode* DCSS(CMNode *d) {
        CMNode *r;
        do {
            r = CAS(d->a2, d->o2, *d);
            if (r && r->isDCSS)
                Complete(r);
        } while (r && r->isDCSS);
        if (r == *(d->o2))
            Complete(d);
        return r;
    }

    void Complete(CMNode *d) {
        CMNode v = READ(*(d->a1));
        if (v == *(d->o1))
            CAS(d->a2, d, *(d->n2));
        else
            CAS(d->a2, d, *(d->o2));
    }

    bool DCAS(CMNode* cd) {
        int status = 0;
        if (READ(*(cd->status)).check == 0) {
            status = 2;
            for (int i = 0; i < 2 && status == 2; ++i) {
                CMNode* val = DCSS(buildDCSS2(cd->status, buildStat(0), i ? cd->a2 : cd->a1, i ? cd->o2 : cd->o1, cd));
                if (val->isDCAS) {
                    if (val != cd) {
                        DCAS(val);
                        i--;
                    }
                }
                else if (val != i ? cd->o2 : cd->o1)
                    status = 1;
            }
            CAS(cd->status, buildStat(0), buildStat(status));
        }
        bool succeeded = (READ(*(cd->status)).check == 2);
        for (i = 0; i < 2; ++i)
            CAS(i ? cd->a2 : cd->a1, cd, succeeded ? i ? *(cd->n2) : *(cd->n1) : i ? *(cd->o2) : *(cd->o1));
        return succeeded;
    }

    CMNode* buildDCSS1(atomic<CMNode>* a2, CMNode* o2, CMNode* n2, atomic<CMNode>* a1, CMNode* o1) {
        return new CMNode { NULL, false, 0, true, false, a1, o1, NULL, a2, o2, n2, 0, NULL, 0 };
    }

    CMNode* buildDCSS2(atomic<CMNode>* a1, CMNode* o1, atomic<CMNode>* a2, CMNode *o2, CMNode* n2) {
        return new CMNode { NULL, false, 0, true, false, a1, o1, NULL, a2, o2, n2, 0, NULL, 0 };
    }

    CMNode* buildDCAS(atomic<CMNode>* a1, CMNode* o1, CMNode* n1, atomic<CMNode>* a2, CMNode *o2, CMNode *n2) {
        atomic<CMNode>* temp = new CMNode { NULL, false, 0, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0};
        return new CMNode { NULL, false, 0, false, true, a1, o1, n1, a2, o2, n2, 0, temp, 0 };
    }

    CMNode* buildStat(int n) {
        return new CMNode { NULL, false, 0, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, n };
    }

    CMNode READ(atomic<CMNode> n) {
        return n;
    }

    int READ(atomic<int> n) {
        return n;
    }

};
