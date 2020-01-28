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

    public <T> T[] filter(Class<T> clazz, T[] a, Predicate<? super T> predicate) {
        int[] flags = new int[a.length + 1];
        IntStream.range(0, a.length).forEach(i -> flags[i + 1] = predicate.test(a[i]) ? 1 : 0);
        Arrays.parallelPrefix(flags, (x, y) -> x + y);
        T[] result = (T[]) Array.newInstance(clazz, flags[flags.length - 1]);
        IntStream.range(0, a.length).forEach(i -> {
            if (flags[i] != flags[i + 1]) result[flags[i]] = a[i];
        });
        return result;
    }

    public double[][] solve() {
        ReentrantLock[] locks = new ReentrantLock[mrf.getNodes()];
        for (int i = 0; i < locks.length; i++) {
            locks[i] = new ReentrantLock();
        }

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

            Thread[] threads = new Thread[this.threads];
            final int len = (messages.length + this.threads - 1) / this.threads;
            for (int i = 0; i < this.threads; i++) {
                int id = i;
                threads[i] = new Thread(() -> {
                    for (int j = len * id; j < Math.min(len * (id + 1), messages.length); j++) {
                        Message message = messages[j];
                        new_mu[message.id] = mrf.getFutureMessage(message);
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

            Message[] reasonableMessages = new Message[0];
            try {
//            reasonableMessages = this.filter(Message.class, messages, m -> Utils.distance(m.logMu, new_mu[m.id]) > sensitivity);
                reasonableMessages = forkJoinPool.submit(() ->
                    this.filter(Message.class, messages, m -> Utils.distance(m.logMu, new_mu[m.id]) > sensitivity)
                ).get();
            } catch (InterruptedException | ExecutionException e) {
                e.printStackTrace();
            }
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

            final int len2 = (reasonableMessages.length + this.threads - 1) / this.threads;

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
//                          }

            oldMessages = newMessages;
        }
        System.out.println(String.format("Updates: %d", updates));
        return mrf.getNodeProbabilities();
    }
}
