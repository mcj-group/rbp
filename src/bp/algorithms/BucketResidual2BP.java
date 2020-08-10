package bp.algorithms;

import bp.MRF.MRF;
import bp.MRF.Message;
import bp.MRF.Utils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Random;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Created by vaksenov on 09.08.2020.
 */
public class BucketResidual2BP extends BPAlgorithm {
    double sensitivity;
    int threads;
    double pV;
    boolean CHECK = false;
    Random rnd = new Random(239);

    public BucketResidual2BP(MRF mrf, int threads, double sensitivity, double pV) {
        super(mrf);
        this.threads = threads;
        this.sensitivity = sensitivity;
        this.pV = pV;
    }

    public interface Iteration {
        public void apply(int i);
    }

    public void parallelFor(int n, Iteration it) {
        Thread[] threads = new Thread[this.threads];
        final int len = (n + this.threads - 1) / this.threads;
        for (int i = 0; i < this.threads; i++) {
            int id = i;
            threads[i] = new Thread(() -> {
                for (int j = len * id; j < Math.min(len * (id + 1), n); j++) {
                    it.apply(j);
                }
            });
            threads[i].start();
        }
        for (int i = 0; i < this.threads; i++) {
            try {
                threads[i].join();
            } catch (InterruptedException e) {
            }
        }
    }

    public interface IterationLR {
        public void apply(int l, int r, int tid);
    }

    public void parallelFor(int n, IterationLR it) {
        Thread[] threads = new Thread[this.threads];
        final int len = (n + this.threads - 1) / this.threads;
        for (int i = 0; i < this.threads; i++) {
            int id = i;
            threads[i] = new Thread(() -> {
                it.apply(len * id, Math.min(len * (id + 1), n), id);
            });
            threads[i].start();
        }
        for (int i = 0; i < this.threads; i++) {
            try {
                threads[i].join();
            } catch (InterruptedException e) {
            }
        }
    }

    public class Vertex implements Comparable<Vertex> {
        int v;
        double priority;

        public Vertex(int v) {
            this.v = v;
        }

        public void recalculatePriority() {
            priority = 0;
            for (Message m : mrf.getMessagesTo(v)) {
                priority = Math.max(priority, Utils.distance(m.logMu, new_mu[m.id]));
            }
        }

        public int compareTo(Vertex v) {
            return Double.compare(v.priority, priority);
        }
    }

    public void swapOrder(int l, int r) {
        Vertex tmp = order[l];
        order[l] = order[r];
        order[r] = tmp;
    }

    public void topK(Vertex[] order, int k) {
        int l = 0;
        int r = order.length;
        while (true) {
            if (r - l <= 2) {
                if (r - l == 2 && order[l + 1].compareTo(order[l]) < 0) {
                    swapOrder(l, l + 1);
                }

                if (CHECK) {
                    // check
                    Vertex best = order[0];
                    for (int i = 0; i < k; i++) {
                        if (order[i].compareTo(best) > 0) {
                            best = order[i];
                        }
                    }

                    boolean good = true;
                    for (int i = 0; i < k; i++) {
                        if (best.compareTo(order[i]) < 0) {
                            good = false;
                        }
                    }
                    for (int i = k + 1; i < order.length; i++) {
                        if (best.compareTo(order[i]) > 0) {
                            good = false;
                        }
                    }

                    if (!good) {
                        throw new AssertionError();
                    }
                }

                return;
            }

            int mid = l + rnd.nextInt(r - l); //(l + r) >> 1;
            swapOrder(l, mid);
            Vertex curr = order[l];
            int i = l;
            int j = r;
            while (true) {
                while (i < j && i < order.length - 1 && order[++i].compareTo(curr) < 0) {
                }
                while (i < j && j > 0 && order[--j].compareTo(curr) > 0) {
                }
                if (i >= j) {
                    break;
                }
                swapOrder(i, j);
            }
            order[l] = order[j];
            order[j] = curr;
            if (j >= k) {
                r = j;
            }
            if (j <= k) {
                l = i;
            }
        }
    }

    private double[][] new_mu;
    private Vertex[] vertices, order;

    @Override
    public double[][] solve() {
        final ArrayList<Message> messages = new ArrayList<>(mrf.getMessages());
        new_mu = new double[messages.size()][];

        parallelFor(messages.size(), (int id) -> {
            Message message = messages.get(id);
            new_mu[message.id] = mrf.getFutureMessage(message);
        });

        Lock[] locks = new ReentrantLock[mrf.getNodes()];
        vertices = new Vertex[mrf.getNodes()];
        for (int i = 0; i < vertices.length; i++) {
            vertices[i] = new Vertex(i);
            locks[i] = new ReentrantLock();
        }

        order = new Vertex[vertices.length];
        int orderK = (int) (pV * vertices.length);
        double[] maxPriority = new double[this.threads];
        int updates = 0;
        int[] updatesThread = new int[this.threads];

        Random rnd = new Random(239);
        ArrayList<Integer> random = new ArrayList<>();
        for (int i = 0; i < mrf.getNodes(); i++) {
            random.add(i);
        }

        while (true) {
            parallelFor(vertices.length, (int l, int r, int tid) -> {
                double p = 0;
                for (int j = l; j < r; j++) {
                    order[j] = vertices[j];
                    vertices[j].recalculatePriority();
                    p = Math.max(p, vertices[j].priority);
                }
                maxPriority[tid] = p;
            });
            double max = 0;
            for (double priority : maxPriority) {
                max = Math.max(max, priority);
            }
            if (max < sensitivity) {
                break;
            }

            topK(order, orderK);

            Collections.shuffle(random, rnd);

            parallelFor(orderK, (int l, int r, int tid) -> {
                int updatesLocal = 0;
                for (int k = l; k < r; k++) {
                    int j = random.get(k);
                    for (Message m : mrf.getMessagesTo(order[j].v)) {
                        int mi = Math.min(m.i, m.j);
                        int mj = Math.max(m.i, m.j);
                        locks[mi].lock();
                        locks[mj].lock();
                        mrf.updateMessage(m, new_mu[m.id]);
                        locks[mi].unlock();
                        locks[mj].unlock();
                        updatesLocal++;
                    }
                }
                updatesThread[tid] = updatesLocal;
            });
            for (int u : updatesThread) {
                updates += u;
            }
            parallelFor(orderK, (int id) -> {
                for (Message m : mrf.getMessagesTo(order[id].v)) {
                    new_mu[m.id] = mrf.getFutureMessage(m);
                }
                for (Message m : mrf.getMessagesFrom(order[id].v)) {
                    new_mu[m.id] = mrf.getFutureMessage(m);
                }
            });
        }

        System.out.println(String.format("Updates: %d", updates));

        return mrf.getNodeProbabilities();
    }
}
