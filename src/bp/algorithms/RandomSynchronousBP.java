package bp.algorithms;

import bp.MRF.MRF;
import bp.MRF.Message;
import bp.MRF.Utils;

import java.lang.reflect.Array;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ForkJoinPool;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.locks.ReentrantLock;
import java.util.function.Predicate;
import java.util.stream.IntStream;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class RandomSynchronousBP extends BPAlgorithm {
    double sensitivity;
    int threads;
    double lowP;
    ForkJoinPool forkJoinPool;
    boolean debug;

    public RandomSynchronousBP(MRF mrf, int threads, double sensitivity, double lowP) {
        super(mrf);
        this.threads = threads;
        this.sensitivity = sensitivity;
        this.lowP = lowP;
        forkJoinPool = new ForkJoinPool(threads);
    }

    public double totalError(Message[] messages) {
        double result = 0;
        for (Message message : messages) {
            result += Utils.distance(message.logMu, mrf.getFutureMessage(message));
        }
        return result;
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

    public interface IterationRange {
        public void apply(int id, int l, int r);
    }

    public void parallelFor(int n, IterationRange it) {
        Thread[] threads = new Thread[this.threads];
        final int len = (n + this.threads - 1) / this.threads;
        for (int i = 0; i < this.threads; i++) {
            int id = i;
            threads[i] = new Thread(() -> {
                it.apply(id, len * id, Math.min(len * (id + 1), n));
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

    public <T> T[] filter(Class<T> clazz, T[] a, Predicate<? super T> predicate) {
        int[] flags = new int[a.length + 1];
        long start = System.currentTimeMillis();
//        IntStream.range(0, a.length).parallel().forEach(i -> flags[i + 1] = predicate.test(a[i]) ? 1 : 0);
        parallelFor(a.length, (int j) -> { flags[j + 1] = predicate.test(a[j]) ? 1 : 0; });
        cerr("Predicate: " + (System.currentTimeMillis() - start));
        start = System.currentTimeMillis();
//        Arrays.parallelPrefix(flags, (x, y) -> x + y);
        for (int i = 1; i < flags.length; i++) {
            flags[i] += flags[i - 1];
        }
        cerr("Parallel prefix: " + (System.currentTimeMillis() - start));
        T[] result = (T[]) Array.newInstance(clazz, flags[flags.length - 1]);
        start = System.currentTimeMillis();
//        IntStream.range(0, a.length).parallel().forEach(i -> {
//            if (flags[i] != flags[i + 1]) result[flags[i]] = a[i];
//        });
        parallelFor(a.length, (int i) -> { if (flags[i] != flags[i + 1]) result[flags[i]] = a[i]; });
//        for (int i = 0; i < a.length; i++) {
//            if (flags[i] != flags[i + 1]) {
//                result[flags[i]] = a[i];
//            }
//        }
        cerr("Copy: " + (System.currentTimeMillis() - start));
        return result;
    }

    public <T> T[] filterSeq(Class<T> clazz, T[] a, Predicate<? super T> predicate) {
        T[] results = (T[]) Array.newInstance(clazz, a.length);
        int l = 0;
        for (int i = 0; i < a.length; i++) {
            if (predicate.test(a[i])) {
                results[l++] = a[i];
            }
        } 
        return Arrays.copyOf(results, l);
    }

    public void cerr(String s) {
        if (debug) {
            System.err.println(s);
        }
    }

    public double[][] solve() {
        debug = false;
        return solveStraightforward();
//        return solveFilter();
    }

    public double[][] solveStraightforward() {
        ReentrantLock[] locks = new ReentrantLock[mrf.getNodes()];
        for (int i = 0; i < locks.length; i++) {
            locks[i] = new ReentrantLock();
        }

        Thread[] threads = new Thread[this.threads];
        final Message[] messages = mrf.getMessages().toArray(new Message[0]);
        double[][] new_mu = new double[messages.length][];
        boolean[] flags = new boolean[messages.length];

        int it = 0;
        int updates = 0;
        int oldMessages = messages.length;
        while (true) {
            if (++it % 10000 == 0) {
                System.err.println(String.format("Current error: %f", totalError(messages)));
            }
            long startIteration = System.currentTimeMillis();

            long start = System.currentTimeMillis();
            parallelFor(messages.length, (int j) -> {
                Message message = messages[j];
                new_mu[message.id] = mrf.getFutureMessage(message);
            });
            cerr("Precalculation: " + (System.currentTimeMillis() - start));


            // Count the number of reasonable messages
            start = System.currentTimeMillis();
            int[] count = new int[this.threads];
            parallelFor(messages.length, (int id, int l, int r) -> {
                int countLocal = 0;
                for (int i = l; i < r; i++) {
                    Message m = messages[i];
                    if (Utils.distance(m.logMu, new_mu[m.id]) > sensitivity) {
//                        flags[m.id] = true;
                        countLocal++;
                    }
                }
                count[id] = countLocal;
            });
            int newMessages = 0;
            for (int i = 0; i < this.threads; i++) {
                newMessages += count[i];
            }
            cerr("Count new messages: " + (System.currentTimeMillis() - start));

            if (newMessages == 0) {
                break;
            }
//            System.err.println(newMessages);

            boolean all = newMessages < 0.9 * oldMessages || newMessages < 100;

            start = System.currentTimeMillis();
            parallelFor(messages.length, (int id, int l, int r) -> {
                int countLocal = 0;
                for (int i = l; i < r; i++) {
                    Message m = messages[i];
                    if (Utils.distance(m.logMu, new_mu[m.id]) > sensitivity && 
                        (all || ThreadLocalRandom.current().nextDouble() < lowP)) {
                        locks[m.j].lock();
                        mrf.updateMessage(m, new_mu[m.id]);
                        locks[m.j].unlock();
//                        mrf.copyMessage(m, new_mu[m.id]);
                        countLocal++;
                    }
                }
                count[id] = countLocal;
            });
            for (int i = 0; i < count.length; i++) {
                updates += count[i];
            }

//            parallelFor(mrf.getNodes(), (int j) -> {
//                mrf.updateNodeSum(j);
//            });

            cerr("Update messages: " + (System.currentTimeMillis() - start));

            oldMessages = newMessages;

            cerr("Iteration: " + (System.currentTimeMillis() - startIteration));
        }
        System.out.println(String.format("Updates: %d", updates));
        return mrf.getNodeProbabilities();
    }

    public double[][] solveFilter() {
        ReentrantLock[] locks = new ReentrantLock[mrf.getNodes()];
        for (int i = 0; i < locks.length; i++) {
            locks[i] = new ReentrantLock();
        }

        Thread[] threads = new Thread[this.threads];
        final Message[] messages = mrf.getMessages().toArray(new Message[0]);
        double[][] new_mu = new double[messages.length][];

        int it = 0;
        int updates = 0;
        int oldMessages = messages.length;
        while (true) {
//            System.err.println(it);
//            if (it == 20) {
//                break;
//            }
            if (++it % 10000 == 0) {
                System.err.println(String.format("Current error: %f", totalError(messages)));
            }

            long startIteration = System.currentTimeMillis();

            long start = System.currentTimeMillis();
            parallelFor(messages.length, (int j) -> {
                Message message = messages[j];
                new_mu[message.id] = mrf.getFutureMessage(message);
            });
            cerr("Precalculation: " + (System.currentTimeMillis() - start));

//        int[] flags = new int[messages.length + 1];
//        start = System.currentTimeMillis();
////        IntStream.range(0, a.length).parallel().forEach(i -> flags[i + 1] = predicate.test(a[i]) ? 1 : 0);
//        parallelFor(messages.length, (int j) -> { flags[j] = Utils.distance(messages[j].logMu, new_mu[messages[j].id]) > sensitivity ? 1 : 0; });
//        cerr("Predicate FUCK: " + (System.currentTimeMillis() - start));

            start = System.currentTimeMillis();
            Message[] reasonableMessages = new Message[0];
            reasonableMessages = this.filter(Message.class, messages, m -> Utils.distance(m.logMu, new_mu[m.id]) > sensitivity);
//            try {
//                reasonableMessages = forkJoinPool.submit(() ->
//                        this.filter(Message.class, messages, m -> Utils.distance(m.logMu, new_mu[m.id]) > sensitivity)
//                ).get();
//            } catch (InterruptedException | ExecutionException e) {
//                e.printStackTrace();
//            }
            cerr("Filter " + (System.currentTimeMillis() - start));

            if (reasonableMessages.length == 0) {
                break;
            }

            int newMessages = reasonableMessages.length;

//            if (newMessages < 0.9 * oldMessages) {
//                // Run synchronous algorithm
//                final int[] updatesThread = new int[this.threads];
//                for (int i = 0; i < this.threads; i++) {
//                    int id = i;
//                    threads[i] = new Thread(() -> {
//                        int updatesLocal = 0;
//                        for (int j = len * id; j < Math.min(len * (id + 1), messages.length); j++) {
//                            Message message = messages[j];
//                            updatesLocal++;
//                            mrf.copyMessage(message, new_mu[message.id]);
//                        }
//                        updatesThread[id] = updatesLocal;
//                    });
//                    threads[i].start();
//                }
//
//                for (int i = 0; i < this.threads; i++) {
//                    try {
//                        threads[i].join();
//                        updates += updatesThread[i];
//                    } catch (InterruptedException e) {
//                    }
//                }
//
//                int lenNodes = (mrf.getNodes() + this.threads - 1) / this.threads;
//                for (int i = 0; i < this.threads; i++) {
//                    int id = i;
//                    threads[i] = new Thread(() -> {
//                        for (int j = lenNodes * id; j < Math.min(lenNodes * (id + 1), mrf.getNodes()); j++) {
//                            mrf.updateNodeSum(j);
//                        }
//                    });
//                    threads[i].start();
//                }
//
//                for (int i = 0; i < this.threads; i++) {
//                    try {
//                        threads[i].join();
//                    } catch (InterruptedException e) {
//                    }
//                }
//            } else {
            if (newMessages > 0.9 * oldMessages && newMessages > 10) {
//                Message[] finalReasonableMessages = reasonableMessages;
//                try {
//                    reasonableMessages = forkJoinPool.submit(() ->
//                            this.filter(Message.class, finalReasonableMessages, m -> ThreadLocalRandom.current().nextDouble() < lowP)
//                    ).get();
//                } catch (InterruptedException | ExecutionException e) {
//                    e.printStackTrace();
//                }
                reasonableMessages = this.filter(Message.class, reasonableMessages, m -> ThreadLocalRandom.current().nextDouble() < lowP);
            }
//            System.err.println(newMessages + ", updated: " + reasonableMessages.length);

            start = System.currentTimeMillis();
            final Message[] finalReasonableMessages = reasonableMessages;
            parallelFor(finalReasonableMessages.length, (int i) -> {
                Message m = finalReasonableMessages[i];
                locks[m.j].lock();
                mrf.updateMessage(m, new_mu[m.id]);
                locks[m.j].unlock();
//                mrf.copyMessage(m, new_mu[m.id]);
            });

            updates += reasonableMessages.length;
            cerr("Copy message: " + (System.currentTimeMillis() - start));

//            start = System.currentTimeMillis();
//            parallelFor(mrf.getNodes(), (int j) -> {
//                mrf.updateNodeSum(j);
//            });
//            cerr("Update nodes: " + (System.currentTimeMillis() - start));

            cerr("Iteration: " + (System.currentTimeMillis() - startIteration));
//                          }
//            System.exit(0);

            oldMessages = newMessages;
        }
        System.out.println(String.format("Updates: %d", updates));
        return mrf.getNodeProbabilities();
    }
}
