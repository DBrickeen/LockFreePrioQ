#include <iostream>
#include <math.h>
#include <atomic>
#include <random>
#include <cmath>
#include <ctime>
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
            return list == r.list && dirty == r.dirty && c == r.c &&
                   isDCSS == r.isDCSS && isDCAS == r.isDCAS && a1 == r.a1 &&
                   o1 == r.o1 && n1 == r.n1 && a2 == r.a2 && o2 == r.o2 &&
                   n2 == r.n2 && count == r.count && status == r.status && check == r.check;
        }
    };

    atomic<CMNode> tree[16][1<<16];
    atomic<int> depth;

public:
    LockFreeMound() {
        //initialize tree
        //each second level array represents a level of the tree

        tree[0][0] = CMNode { NULL, false, 0, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0 };
        depth = 1;
        srand(time(0));
    }

    T value(CMNode n) {
        return n->list ? n->list->value : maxValue;
    }

    void insert(T value) {
        while (true) {
            int n = findInsertPoint(value);
            int x = indexX(n), y = indexY(n, x);
            CMNode* C = DCASRead(tree[x] + y);
            if (value(*C) >= value) {
                CMNode CP = CMNode { new LNode { value, C->list }, C->dirty, C->c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0 };
                if (n == 1) {
                    if (tree[x][y].compare_exchange_weak(C, CP))
                        return;
                    else {
                        int p = (n - 1) / 2;
                        int px = indexX(p), py = indexY(p, px);
                        CMNode* P = DCASRead(tree[px] + py);
                        if (value(*P) <= value) {
                            CMNode* desc = buildDCSS1(tree[x] + y, C, &CP, tree[px] + py, P);
                            if (DCSS(desc)) {
                                delete(desc);
                                return;
                            }
                            delete(desc);
                        }
                    }
                    delete(CP.list);
                }
            }
        }
    }

    int findInsertPoint(T v) {
        while (true) {
            int d = READ(depth);
            for (int i = 0; i < THRESHOLD; ++i) {
                int n = randLeaf(d);
                if (value(n) >= v)
                    return bs(n, v);
            }
            atomic_compare_exchange_weak(&depth, &d, d + 1);
        }
    }

    // i realized this actually is log(n) runtime, i kept thinking we needed log(log(n)) (which is doable but unnecessary)
    int bs(int n, int v) {
        if (n == 0)
            return n;
        int p = (n - 1) / 2;
        int px = indexX(p), py = indexY(p, px);
        if (value(tree[px][py]) < v)
            return n;
        return bs(p, v);
    }

    int randLeaf() {
        return (int) rand() % (int) pow(2, depth - 1)
            + (int) pow(2, depth - 1) - 1;
    }

    T extractMin() {
        while (true) {
            CMNode* R = DCASRead(tree[0] + 0);
            if (R->dirty) {
                moundify(1);
                continue;
            }
            if (R->list == NULL)
                return maxValue;
            if (tree[0][0].compare_exchange_weak(R, CMNode {R->list->next, true, R->c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0})) {
                T retval = R->list->value;
                delete(R->list);
                moundify(1);
                return retval;
            }
        }
    }

    void moundify(int n) {
        while (true) {
            int x = indexX(n), y = indexY(n, x);
            CMNode* N = DCASRead(tree[x] + y);
            int d = READ(depth);
            if (!N->dirty)
                return;
            if (n >= (1<<(d-1)) && n <= (1<<d) - 1)
                return;
            int lx = indexX(2 * n), ly = indexY(2 * n, lx);
            CMNode* L = DCASRead(tree[lx] + ly);
            int rx = indexX(2 * n + 1), ry = indexY(2 * n + 1, rx);
            CMNode* R = DCASRead(tree[rx] + ry);
            if (L->dirty) {
                moundify(2 * n);
                continue;
            }
            if (R->dirty) {
                moundify(2 * n + 1);
                continue;
            }
            if (value(*L) <= value(*R) && value(*L) < value(*N)) {
                CMNode* desc = buildDCAS(tree[x] + y, N, new CMNode {L->list, false, N->c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0 },
                                   tree[lx] + ly, L, new CMNode {N->list, true, L->c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0 });
                if (DCAS(desc)) {
                    delete(desc->status);
                    delete(desc->n1);
                    delete(desc->n2);
                    delete(desc);
                    moundify(2 * n);
                    return;
                }
            }
            else if (value(*R) < value(*L) && value(*R) < value(*N)) {
                CMNode* desc = buildDCAS(tree[x] + y, N, new CMNode {R->list, false, N->c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0},
                                   tree[rx] + ry, R, new CMNode {N->list, true, R->c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0});
                if (DCAS(desc)) {
                    delete(desc->status);
                    delete(desc->n1);
                    delete(desc->n2);
                    delete(desc);
                    moundify(2 * n + 1);
                    return;
                }
            }
            else if (tree[x][y].compare_exchange_weak(N, CMNode {N->list, false, N->c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}))
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
        CMNode *r = NULL;
        do {
            r = CAS(d->a2, d->o2, *d);
            if (r && r->isDCSS)
                Complete(r);
        } while (r && r->isDCSS);
        if (r == *(d->o2))
            Complete(d);
        return r;
    }

    CMNode* DCSSRead(CMNode* d) {
        CMNode* r = NULL;
        do {
            r = CAS(d->a2, d->o2, *d);
            if (r && r->isDCSS) Complete(r);
        } while (r && r->isDCSS);
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
        for (int i = 0; i < 2; ++i)
            CAS(i ? cd->a2 : cd->a1, cd, succeeded ? i ? *(cd->n2) : *(cd->n1) : i ? *(cd->o2) : *(cd->o1));
        return succeeded;
    }

    CMNode* DCASRead(CMNode* addr) {
        CMNode* r = NULL;
        do {
            r = DCSSRead(addr);
            if (r && r->isDCAS) DCAS(r);
        } while (r && r->isDCAS);
        return r;
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
