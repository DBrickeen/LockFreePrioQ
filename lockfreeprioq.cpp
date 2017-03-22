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
    struct CMNode {
        LNode* list;  //sorted list of values stored at this node
        bool dirty;   //true if mound property does not hold
        int c;        //counter - incremented on every update
    };

    atomic<CMNode> tree[MAX_DEPTH][];
    unsigned int depth;

public:
    LockFreeMound() {
        //initialize tree
        //each second level array represents a level of the tree
        for (int i = 0; i < MAX_DEPTH; ++i) {
            tree[i] = CMNode[1<<i];
        }

        tree[0][0] = CMNode { NULL, false, 0 };
        depth = 1;
    }

    T value(CMNode n) {
        return n.list ? n.list->value : maxValue;
    }

    void insert(T value) {
        while (true) {
            int n = findInsertPoint(value);
            int x = indexX(n), y = indexY(n, x);
            CMNode C = READ(tree[x][y]);
            if (C >= value) {
                CNode<T>* newCNode = { new LNode<T> { v, tree[x][y].list }, C.dirty, c + 1 };
                if (n == 1) {
                    if (CAS(tree, C, newCNode))
                        return;
                    else 
                }
            }
        }
    }

    int findInsertPoint(T v) {

    }

    int randLeaf() {

    }

    T extractMin() {

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

    bool CAS(atomic<CMNode> tree, LNode C, LNode CP) {

    }
};
