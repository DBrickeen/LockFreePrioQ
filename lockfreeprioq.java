import java.util.*;
import java.util.concurrent.atomic.*;
import java.io.*;
public class prioq<T extends Comparable<T>> {
	AtomicReference<CMNode>[] tree;
	AtomicInteger depth;
	Random r;
	T max;
	CMNode undecide, failed, success;
	class CMNode {
		Stack<T> list;
		boolean dirty;
		int c;
		boolean isDCSS;
		boolean isDCAS;
		AtomicReference<CMNode> a1;
		CMNode o1;
		CMNode n1;
		AtomicReference<CMNode> a2;
		CMNode o2;
		CMNode n2;
		int count;
		AtomicReference<CMNode> status;
		int check;
		CMNode(AtomicReference<CMNode> a, CMNode b, AtomicReference<CMNode> d, CMNode e, CMNode f) {
			a1 = a;
			o1 = b;
			n2 = null;
			a2 = d;
			o2 = e;
			n2 = f;
			isDCSS = true;
			isDCAS = false;
			list = null;
			status = null;
			c = check = count = 0;
			dirty = false;
		}
		CMNode(AtomicReference<CMNode> a, CMNode b, CMNode d, AtomicReference<CMNode> e, CMNode f, CMNode g) {
			a1 = a;
			o1 = b;
			n1 = d;
			a2 = e;
			o2 = f;
			n2 = g;
			isDCSS = false;
			isDCAS = true;
			list = null;
			status = new AtomicReference<CMNode>(undecide);
			c = check = count = 0;
			dirty = false;
		}
		CMNode(int a) {
			a1 = null;
			o1 = null;
			n1 = null;
			a2 = null;
			o2 = null;
			n2 = null;
			isDCSS = false;
			isDCAS = false;
			list = null;
			status = null;
			c = count = 0;
			check = a;
			dirty = false;
		}
		CMNode(CMNode N) {
			a1 = N.a1;
			o1 = N.o1;
			n1 = N.n1;
			a2 = N.a2;
			o2 = N.o2;
			n2 = N.n2;
			isDCSS = N.isDCSS;
			isDCAS = N.isDCAS;
			list = (Stack<T>) N.list.clone();
			status = N.status;
			c = N.c;
			check = N.check;
			count = N.count;
			dirty = N.dirty;
		}
		CMNode() {
			dirty = false;
			list = new Stack<T>();
			c = 0;
			a1 = null;
			o1 = null;
			n1 = null;
			a2 = null;
			o2 = null;
			n2 = null;
			isDCSS = false;
			isDCAS = false;
			check = 0;
			count = 0;
			status = null;
		}
	}
	prioq(T m) {
		tree = new AtomicReference[16 * (1<<16)];
		for (int i = 0; i < tree.length; ++i)
			tree[i] = new AtomicReference<CMNode>(new CMNode());
		depth = new AtomicInteger(0);
		r = new Random();
		max = m;
		undecide = new CMNode(0);
		failed = new CMNode(1);
		success = new CMNode(2);
	}
	T getval(CMNode n) {
		return n == null || n.list == null || n.list.isEmpty() ? max : n.list.peek();
	}
	void insert(T val) {
		while (true) {
			int n = findInsertPoint(val);
			CMNode C = Read(tree[n].get());
			if (getval(C).compareTo(val) >= 0) {
				CMNode CP = new CMNode(C);
				CP.list.push(val);
				CP.c++;
				if (n == 0) {
					if (tree[n].compareAndSet(C, CP))
						return;
				}
				else {
					int p = (n - 1) / 2;
					CMNode P = Read(tree[p].get());
					if (getval(P).compareTo(val) <= 0) {
						if (DCAS(new CMNode(tree[p], P, P, tree[n], C, CP))) {
							return;
						}
					}
				}
			}
		}
	}
	int findInsertPoint(T val) {
		while (true) {
			int d = depth.get();
			for (int i = 0; i < 8; ++i) {
				int n = randLeaf(d);
				CMNode N = Read(tree[n].get());
				if (getval(N).compareTo(val) >= 0)
					return bs(n, val);
			}
			depth.compareAndSet(d, d + 1);
		}
	}
	int bs(int n, T val) {
		if (n == 0)
			return n;
		int p = (n - 1) / 2;
		CMNode N = Read(tree[p].get());
		if (getval(N).compareTo(val) < 0)
			return n;
		return bs(p, val);
	}
	int randLeaf(int d) {
		if (d == 0)
			return 0;
		int lo = (1<<d) - 1;
		int hi = ((1<<(d + 1)) - 1);
		return lo + (Math.abs(r.nextInt()) % (hi - lo));
	}
	boolean isLeaf(int d, int n) {
		if (d == 0)
			return n == 0;
		int lo = (1<<d) - 1;
		return n >= lo;
	}
	T extractMin() {
		while (true) {
			CMNode R = Read(tree[0].get());
			if (R.dirty) {
				moundify(0);
				continue;
			}
			if (R.list.isEmpty())
				return max;
			CMNode RP = new CMNode(R);
			RP.list.pop();
			RP.dirty = true;
			RP.c++;
			if (tree[0].compareAndSet(R, RP)) {
				T ret = R.list.peek();
				moundify(0);
				return ret;
			}
		}
	}
	void moundify(int n) {
		while (true) {
			CMNode N = Read(tree[n].get());
			int d = depth.get();
			if (!N.dirty)
				return;
			if (isLeaf(d, n)) {
				CMNode undirty = new CMNode(N);
				undirty.dirty = false;
				undirty.c++;
				tree[n].compareAndSet(N, undirty);
				return;
			}
			CMNode L = Read(tree[2 * n + 1].get());
			CMNode R = Read(tree[2 * n + 2].get());
			if (L.dirty) {
				moundify(2 * n + 1);
				continue;
			}
			if (R.dirty) {
				moundify(2 * n + 2);
				continue;
			}
			if (getval(L).compareTo(getval(R)) <= 0 && getval(L).compareTo(getval(N)) < 0) {
				CMNode LP = new CMNode(L);
				LP.dirty = false;
				LP.c = N.c + 1;
				CMNode NP = new CMNode(N);
				NP.dirty = true;
				NP.c = L.c + 1;
				if (DCAS(new CMNode(tree[n], N, LP, tree[2 * n + 1], L, NP))) {
					moundify(2 * n + 1);
					return;
				}
			}
			else if (getval(R).compareTo(getval(L)) < 0 && getval(R).compareTo(getval(N)) < 0) {
				CMNode RP = new CMNode(R);
				RP.dirty = false;
				RP.c = N.c + 1;
				CMNode NP = new CMNode(N);
				NP.dirty = true;
				NP.c = R.c + 1;
				if (DCAS(new CMNode(tree[n], N, RP, tree[2 * n + 2], R, NP))) {
					moundify(2 * n + 2);
					return;
				}
			}
			else {
				CMNode NP = new CMNode(N);
				NP.dirty = false;
				NP.c++;
				if (tree[n].compareAndSet(N, NP))
					return;
			}
		}
	}
	CMNode CAS(AtomicReference<CMNode> N, CMNode C, CMNode CP) {
		if (N != null && N.compareAndSet(C, CP))
			return C;
		return C;
	}
	CMNode DCSS(CMNode d) {
		CMNode r;
		do {
			r = CAS(d.a2, d.o2, d);
			if (r.isDCSS)
				Complete(r);
		} while (r.isDCSS);
		if (r == d.o2)
			Complete(d);
		return r;
	}
	CMNode DCSSRead(CMNode d) {
		CMNode r;
		do {
			r = CAS(d.a2, d.o2, d);
			if (r != null && r.isDCSS)
				Complete(r);
		} while (r != null && r.isDCSS);
		return r;
	}
	void Complete(CMNode d) {
		CMNode v = d.a1.get();
		if (v == d.o1)
			d.a2.compareAndSet(d, d.n2);
		else
			d.a2.compareAndSet(d, d.o2);
	}
	boolean DCAS(CMNode d) {
		int status = 0;
		if (d.status.get().check == 0) {
			status = 2;
			for (int i = 0; i < 2 && status == 2; ++i) {
				CMNode val = DCSS(new CMNode(d.status, getstatus(0), i != 0 ? d.a2 : d.a1, i != 0 ? d.o2 : d.o1, d));
				if (val.isDCAS) {
					if (val != d) {
						DCAS(val);
						--i;
					}
				}
				else if (val != (i != 0 ? d.o2 : d.o1)) {
					status = 1;
				}
			}
			d.status.compareAndSet(getstatus(0), getstatus(status));
		}
		boolean succeeded = (d.status.get().check == 2);
		for (int i = 0; i < 2; ++i)
			(i != 0 ? d.a2 : d.a1).compareAndSet(d, succeeded ? i != 0 ? d.n2 : d.n1 : i != 0 ? d.o2 : d.o1);
		return succeeded;
	}
	CMNode DCASRead(CMNode addr) {
		CMNode r;
		do {
			r = DCSSRead(addr);
			if (r != null && r.isDCAS)
				DCAS(r);
		} while (r != null && r.isDCAS);
		return r;
	}
	CMNode Read(CMNode N) {
		CMNode r = DCASRead(N);
		if (r == null)
			return N;
		return r;
	}
	CMNode getstatus(int n) {
		if (n == 0)
			return undecide;
		if (n == 1)
			return failed;
		return success;
	}
}
