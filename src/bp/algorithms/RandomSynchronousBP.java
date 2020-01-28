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

    public <T> T[] filter(Class<T> clazz, T[] a, Predicate<? super T> predicate) {
        int[] flags = new int[a.length + 1];
        long start = System.currentTimeMillis();
//        IntStream.range(0, a.length).parallel().forEach(i -> flags[i + 1] = predicate.test(a[i]) ? 1 : 0);
        parallelFor(a.length, (int j) -> { flags[j] = predicate.test(a[j]) ? 1 : 0; });
        System.err.println("Predicate: " + (System.currentTimeMillis() - start));
        start = System.currentTimeMillis();
//        Arrays.parallelPrefix(flags, (x, y) -> x + y);
        for (int i = 1; i < flags.length; i++) {
            flags[i] += flags[i - 1];
        }
        System.err.println("Parallel prefix: " + (System.currentTimeMillis() - start));
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
        System.err.println("Copy: " + (System.currentTimeMillis() - start));
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

    public double[][] solve() {
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

            long start = System.currentTimeMillis();
            parallelFor(messages.length, (int j) -> {
                Message message = messages[j];
                new_mu[message.id] = mrf.getFutureMessage(message);
            });
            System.err.println(System.currentTimeMillis() - start);

        int[] flags = new int[messages.length + 1];
        start = System.currentTimeMillis();
//        IntStream.range(0, a.length).parallel().forEach(i -> flags[i + 1] = predicate.test(a[i]) ? 1 : 0);
        parallelFor(messages.length, (int j) -> { flags[j] = Utils.distance(messages[j].logMu, new_mu[messages[j].id]) > sensitivity ? 1 : 0; });
        System.err.println("Predicate FUCK: " + (System.currentTimeMillis() - start));

            start = System.currentTimeMillis();
            Message[] reasonableMessages = new Message[0];
            reasonableMessages = this.filter(Message.class, messages, m -> Utils.distance(m.logMu, new_mu[m.id]) > sensitivity);
/*            try {
                reasonableMessages = forkJoinPool.submit(() ->
                        this.filter(Message.class, messages, m -> Utils.distance(m.logMu, new_mu[m.id]) > sensitivity)
                ).get();
            } catch (InterruptedException | ExecutionException e) {
                e.printStackTrace();
            }*/
            System.err.println("Filter " + (System.currentTimeMillis() - start));

//            for (Message m : reasonableMessages) {
//                if (Utils.distance(m.logMu, new_mu[m.id]) < sensitivity) {
//                    throw new AssertionError();
//                }
//            }

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
            if (newMessages > 0.9 * oldMessages && newMessages > 100) {
                Message[] finalReasonableMessages = reasonableMessages;
                try {
                    reasonableMessages = forkJoinPool.submit(() ->
                            this.filter(Message.class, finalReasonableMessages, m -> ThreadLocalRandom.current().nextDouble() < lowP)
                    ).get();
                } catch (InterruptedException | ExecutionException e) {
                    e.printStackTrace();
                }
            }
            System.err.println(reasonableMessages.length);

            final int len2 = (reasonableMessages.length + this.threads - 1) / this.threads;

            start = System.currentTimeMillis();
            final int[] updatesThread = new int[this.threads];
            for (int i = 0; i < this.threads; i++) {
                int id = i;
                Message[] finalReasonableMessages = reasonableMessages;
                threads[i] = new Thread(() -> {
                    int updatesLocal = 0;
                    for (int j = len2 * id; j < Math.min(len2 * (id + 1), finalReasonableMessages.length); j++) {
                        Message message = finalReasonableMessages[j];
                        locks[message.j].lock();
                        mrf.updateMessage(message, new_mu[message.id]);
                        locks[message.j].unlock();
//                        mrf.copyMessage(message, new_mu[message.id]);
                        updatesLocal++;
                    }
                    updatesThread[id] = updatesLocal;
                });
                threads[i].start();
            }

            for (int i = 0; i < this.threads; i++) {
                try {
                    threads[i].join();
                    updates += updatesThread[i];
                } catch (InterruptedException e) {
                }
            }
            System.err.println("Copy message: " + (System.currentTimeMillis() - start));

/*            start = System.currentTimeMillis();
            parallelFor(mrf.getNodes(), (int j) -> {
                mrf.updateNodeSum(j);
            });
            System.err.println("Update nodes: " + (System.currentTimeMillis() - start));*/
/*            int lenNodes = (mrf.getNodes() + this.threads - 1) / this.threads;
            for (int i = 0; i < this.threads; i++) {
                int id = i;
                threads[i] = new Thread(() -> {
                    for (int j = lenNodes * id; j < Math.min(lenNodes * (id + 1), mrf.getNodes()); j++) {
                        mrf.updateNodeSum(j);
                    }
                });
                threads[i].start();
            }

            for (int i = 0; i < this.threads; i++) {
                try {
                    threads[i].join();
                } catch (InterruptedException e) {
                }
            }*/

//                          }
            System.exit(0);

            oldMessages = newMessages;
        }
        System.out.println(String.format("Updates: %d", updates));
        return mrf.getNodeProbabilities();
    }
}
