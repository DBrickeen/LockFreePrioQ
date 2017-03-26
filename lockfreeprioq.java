import java.util.*;
import java.util.concurrent.atomic.*;
public class prioq<T extends Comparable<T>> {
	AtomicReference<CMNode>[] tree;
	AtomicInteger depth;
	Random r;
	T max;
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
			o1 = copy(b);
			a2 = d;
			o2 = copy(e);
			n2 = copy(f);
			isDCSS = true;
			isDCAS = false;
			list = null;
			status = null;
			c = check = count = 0;
			dirty = false;
		}
		CMNode(AtomicReference<CMNode> a, CMNode b, CMNode d, AtomicReference<CMNode> e, CMNode f, CMNode g) {
			a1 = a;
			o1 = copy(b);
			n1 = copy(d);
			a2 = e;
			o2 = copy(f);
			n2 = copy(g);
			isDCSS = false;
			isDCAS = true;
			list = null;
			status = new AtomicReference<CMNode>(new CMNode(0));
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
			o1 = copy(N.o1);
			n1 = copy(N.n1);
			a2 = N.a2;
			o2 = copy(N.o2);
			n2 = copy(N.n2);
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
		public boolean equals(CMNode N) {
			if (isDCSS != N.isDCSS || isDCAS != N.isDCAS)
				return false;
			if (isDCSS)
				return a1.equals(N.a1) && o1.equals(N.o1) &&
					   a2.equals(N.a2) && o2.equals(N.o2) &&
					   n2.equals(N.n2);
			if (isDCAS)
				return a1.equals(N.a1) && o1.equals(N.o1) &&
					   a2.equals(N.a2) && o2.equals(N.o2) &&
					   n2.equals(N.n2) && n1.equals(N.n1) &&
					   status.equals(N.status);
			return list.equals(N.list) && check == N.check &&
				   count == N.count && c == N.c && dirty == N.dirty;
		}
	}
	prioq() {
		tree = new AtomicReference[1<<16];
		tree[0] = new AtomicReference<CMNode>(new CMNode());
		depth.set(1);
		r = new Random();
	}
	CMNode copy(CMNode N) {
		if (N == null)
			return null;
		CMNode ret = new CMNode();
		ret.a1 = N.a1;
		ret.a2 = N.a2;
		ret.o1 = copy(N.o1);
		ret.o2 = copy(N.o2);
		ret.n1 = copy(N.n1);
		ret.n2 = copy(N.n2);
		ret.c = N.c;
		ret.check = N.check;
		ret.count = N.count;
		ret.dirty = N.dirty;
		ret.isDCSS = N.isDCSS;
		ret.isDCAS = N.isDCAS;
		ret.status = N.status;
		ret.list = (Stack<T>) N.list.clone();
		return ret;
	}
	T getval(CMNode n) {
		return n.list.size() > 0 ? n.list.peek() : max;
	}
	void insert(T val) {
		while (true) {
			int n = findInsertPoint(val);
			CMNode C = DCASRead(tree[n].get());
			if (getval(C).compareTo(val) >= 0) {
				CMNode CP = new CMNode(C);
				CP.list.push(val);
				if (n == 1) {
					if (tree[n].compareAndSet(C, CP))
						return;
					else {
						int p = (n - 1) / 2;
						CMNode P = DCASRead(tree[p].get());
						if (getval(P).compareTo(val) <= 0) {
							if (DCAS(new CMNode(tree[p], P, P, tree[n], C, CP)))
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
				CMNode N = DCASRead(tree[n].get());
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
		CMNode N = DCASRead(tree[p].get());
		if (getval(N).compareTo(val) < 0)
			return n;
		return bs(p, val);
	}
	int randLeaf(int d) {
		return (int) r.nextInt() % (int) (1<<d-1) + (int) (1<<d-1) - 1;
	}
	T extractMin() {
		while (true) {
			CMNode R = DCASRead(tree[0].get());
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
			CMNode N = DCASRead(tree[n].get());
			int d = depth.get();
			if (!N.dirty)
				return;
			if (n >= (1<<(d-1)) && n <= (1<<d) - 1)
				return;
			CMNode L = DCASRead(tree[2 * n + 1].get());
			CMNode R = DCASRead(tree[2 * n + 2].get());
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
		if (N.compareAndSet(C, CP))
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
		if (r.equals(d.o2))
			Complete(d);
		return r;
	}
	CMNode DCSSRead(CMNode d) {
		CMNode r;
		do {
			r = CAS(d.a2, d.o2, d);
			if (r.isDCSS)
				Complete(r);
 		} while (r.isDCSS);
		return r;
	}
	void Complete(CMNode d) {
		CMNode v = d.a1.get();
		if (v.equals(d.o1))
			d.a2.compareAndSet(d, d.n2);
		else
			d.a2.compareAndSet(d, d.o2);
	}
	boolean DCAS(CMNode d) {
		int status = 0;
		if (d.status.get().check == 0) {
			status = 2;
			for (int i = 0; i < 2 && status == 2; ++i) {
				CMNode val = DCSS(new CMNode(d.status, new CMNode(0), i != 0 ? d.a2 : d.a1, i != 0 ? d.o2 : d.o1, d));
				if (val.isDCAS) {
					if (val.equals(d)) {
						DCAS(val);
						--i;
					}
				}
				else if (!val.equals(i != 0 ? d.o2 : d.o1))
					status = 1;
			}
			d.status.compareAndSet(new CMNode(0), new CMNode(status));
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
			if (r.isDCAS)
				DCAS(r);
		} while (r.isDCAS);
		return r;
	}
}
