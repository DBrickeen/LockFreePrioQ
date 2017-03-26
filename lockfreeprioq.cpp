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

    T getvalue(CMNode n) {
        return n.list ? n.list->value : maxValue;
    }

    void insert(T value) {
        while (true) {
            int n = findInsertPoint(value);
            int x = indexX(n), y = indexY(n, x);
            CMNode C = DCASRead(tree[x] + y);
            if (getvalue(C) >= value) {
                CMNode CP = CMNode { new LNode { value, C.list }, C.dirty, C.c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0 };
                if (n == 1) {
                    if (atomic_compare_exchange_weak(tree[x] + y, &C, CP))
                        return;
                    else {
                        int p = (n - 1) / 2;
                        int px = indexX(p), py = indexY(p, px);
                        CMNode P = DCASRead(tree[px] + py);
                        if (getvalue(P) <= value) {
                            atomic<CMNode>* desc = buildDCSS1(tree[x] + y, &C, &CP, tree[px] + py, &P);
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
            int d = depth;
            for (int i = 0; i < THRESHOLD; ++i) {
                int n = randLeaf(d);
                int x = indexX(n), y = indexY(n, x);
                CMNode N = DCASRead(tree[x] + y);
                if (getvalue(N) >= v)
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
        if (getvalue(tree[px][py]) < v)
            return n;
        return bs(p, v);
    }

    int randLeaf(int d) {
        return (int) rand() % (int) pow(2, d - 1)
            + (int) pow(2, d - 1) - 1;
    }

    T extractMin() {
        while (true) {
            CMNode R = DCASRead(tree[0] + 0);
            if (R.dirty) {
                moundify(1);
                continue;
            }
            if (R.list == NULL)
                return maxValue;
            if (atomic_compare_exchange_weak(tree[0] + 0, &R, CMNode {R.list->next, true, R.c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0})) {
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
            CMNode N = DCASRead(tree[x] + y);
            int d = depth;
            if (!N.dirty)
                return;
            if (n >= (1<<(d-1)) && n <= (1<<d) - 1)
                return;
            int lx = indexX(2 * n), ly = indexY(2 * n, lx);
            CMNode L = DCASRead(tree[lx] + ly);
            int rx = indexX(2 * n + 1), ry = indexY(2 * n + 1, rx);
            CMNode R = DCASRead(tree[rx] + ry);
            if (L.dirty) {
                moundify(2 * n);
                continue;
            }
            if (R.dirty) {
                moundify(2 * n + 1);
                continue;
            }
            if (getvalue(L) <= getvalue(R) && getvalue(L) < getvalue(N)) {
                atomic<CMNode>* desc = buildDCAS(tree[x] + y, &N, new CMNode {L.list, false, N.c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0 },
                                   tree[lx] + ly, &L, new CMNode {N.list, true, L.c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0 });
                if (DCAS(desc)) {
                    delete(desc->load().status);
                    delete(desc->load().n1);
                    delete(desc->load().n2);
                    delete(desc);
                    moundify(2 * n);
                    return;
                }
            }
            else if (getvalue(R) < getvalue(L) && getvalue(R) < getvalue(N)) {
                atomic<CMNode>* desc = buildDCAS(tree[x] + y, &N, new CMNode {R.list, false, N.c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0},
                                   tree[rx] + ry, &R, new CMNode {N.list, true, R.c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0});
                if (DCAS(desc)) {
                    delete(desc->load().status);
                    delete(desc->load().n1);
                    delete(desc->load().n2);
                    delete(desc);
                    moundify(2 * n + 1);
                    return;
                }
            }
            else if (atomic_compare_exchange_weak(tree[x] + y, &N, CMNode {N.list, false, N.c + 1, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}))
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
        if (atomic_compare_exchange_weak(tree, C, CP))
            return C;
        return C;
    }

    CMNode* DCSS(atomic<CMNode>* d) {
        CMNode* r = NULL;
        do {
            r = CAS(d->load().a2, d->load().o2, *d);
            if (r->isDCSS) {
                atomic<CMNode>* temp = new atomic<CMNode> { *r };
                Complete(temp);
                delete(temp);
            }
        } while (r->isDCSS);
        if (*r == *(d->load().o2))
            Complete(d);
        return r;
    }

    CMNode* DCSSRead(atomic<CMNode>* d) {
        CMNode* r = NULL;
        CMNode dl = d->load();
        do {
            r = CAS(dl.a2, dl.o2, *d);
            if (r->isDCSS) {
                atomic<CMNode>* temp = new atomic<CMNode> { *r };
                Complete(temp);
                delete(temp);
            }
        } while (r->isDCSS);
        return r;
    }

    void Complete(atomic<CMNode>* d) {
        CMNode dl = d->load();
        CMNode v = dl.a1->load();
        if (v == *(dl.o1))
            CAS(dl.a2, &dl, *(dl.n2));
        else
            CAS(dl.a2, &dl, *(dl.o2));
    }

    bool DCAS(atomic<CMNode>* cd) {
        int status = 0;
        CMNode cdl = cd->load();
        if (cdl.status->load().check == 0) {
            status = 2;
            for (int i = 0; i < 2 && status == 2; ++i) {
                CMNode* val = DCSS(buildDCSS2(cdl.status, buildStat(0), i ? cdl.a2 : cdl.a1, i ? cdl.o2 : cdl.o1, &cdl));
                if (val->isDCAS) {
                    if (val == &cdl) {
                        atomic<CMNode>* temp = new atomic<CMNode> { *val };
                        DCAS(temp);
                        delete(temp);
                        i--;
                    }
                }
                else if (val != (i ? cdl.o2 : cdl.o1))
                    status = 1;
            }
            CAS(cdl.status, buildStat(0), *buildStat(status));
        }
        bool succeeded = (cdl.status->load().check == 2);
        for (int i = 0; i < 2; ++i)
            CAS(i ? cdl.a2 : cdl.a1, &cdl, succeeded ? i ? *(cdl.n2) : *(cdl.n1) : i ? *(cdl.o2) : *(cdl.o1));
        return succeeded;
    }

    CMNode DCASRead(atomic<CMNode>* addr) {
        CMNode* r = NULL;
        do {
            r = DCSSRead(addr);
            if (r->isDCAS) {
                atomic<CMNode>* temp = new atomic<CMNode> { *r };
                DCAS(temp);
                delete(temp);
            }
        } while (r->isDCAS);
        return *r;
    }

    atomic<CMNode>* buildDCSS1(atomic<CMNode>* a2, CMNode* o2, CMNode* n2, atomic<CMNode>* a1, CMNode* o1) {
        return new atomic<CMNode> { CMNode { NULL, false, 0, true, false, a1, o1, NULL, a2, o2, n2, 0, NULL, 0 } };
    }

    atomic<CMNode>* buildDCSS2(atomic<CMNode>* a1, CMNode* o1, atomic<CMNode>* a2, CMNode *o2, CMNode* n2) {
        return new atomic<CMNode> { CMNode { NULL, false, 0, true, false, a1, o1, NULL, a2, o2, n2, 0, NULL, 0 } };
    }

    atomic<CMNode>* buildDCAS(atomic<CMNode>* a1, CMNode* o1, CMNode* n1, atomic<CMNode>* a2, CMNode *o2, CMNode *n2) {
        atomic<CMNode>* temp = new atomic<CMNode> { CMNode { NULL, false, 0, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0} };
        return new atomic<CMNode> { CMNode { NULL, false, 0, false, true, a1, o1, n1, a2, o2, n2, 0, temp, 0 } };
    }

    CMNode* buildStat(int n) {
        return new CMNode { NULL, false, 0, false, false, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, n };
    }
};
