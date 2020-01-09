package bp.algorithms;

import bp.MRF.Message;
import bp.MRF.MRF;
import bp.MRF.Utils;
import bp.algorithms.queues.SequentialPQ;

import java.util.Collection;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class ConcurrentResidualBP extends BPAlgorithm {
    int threads;
    boolean fair;
    double sensitivity;

    public ConcurrentResidualBP(MRF mrf, int threads, boolean fair, double sensitivity) {
        super(mrf);
        this.threads = threads;
        this.sensitivity = sensitivity;
        this.fair = fair;
    }

    private double getPriority(Message e) {
        return Utils.distance(e.logMu, mrf.getFutureMessage(e));
    }

    public double[][] solve() {
        ReentrantLock[] locks = new ReentrantLock[mrf.getNodes()];
        for (int i = 0; i < locks.length; i++) {
            locks[i] = new ReentrantLock();
        }
        Collection<Message> messages = mrf.getMessages();
        final SequentialPQ<Message> priorityQueue = new SequentialPQ<>(messages.size());
        for (Message message : messages) {
            priorityQueue.insert(message, getPriority(message));
        }
        AtomicInteger iterations = new AtomicInteger();
        Thread[] workers = new Thread[threads];
        for (int i = 0; i < workers.length; i++) {
            workers[i] = new Thread(() -> {
                int it = 0;
                while (true) {
                    if (++it % 1000 == 0) {
                        synchronized (priorityQueue) {
                            if (priorityQueue.peek().priority < sensitivity) {
                                iterations.addAndGet(it);
                                return;
                            }
                        }
                    }
                    Message m;
                    synchronized (priorityQueue) {
                        m = priorityQueue.extractMin();
                    }

                    int mi = Math.min(m.i, m.j);
                    int mj = Math.max(m.i, m.j);
                    if (fair) {
                        locks[mi].lock();
                        locks[mj].lock();
                    }
                    mrf.updateMessage(m);

                    Collection<Message> messagesFromJ = mrf.getMessagesFrom(m.j);
                    double[] newPriorities = new double[messagesFromJ.size()];
                    int j = 0;
                    for (Message affected : messagesFromJ) {
                        newPriorities[j] = getPriority(affected);
                        j++;
                    }

                    synchronized (priorityQueue) {
                        j = 0;
                        for (Message affected : messagesFromJ) {
                            priorityQueue.changePriority(affected, newPriorities[j]);
                            j++;
                        }
                        priorityQueue.insert(m, 0);
//                        if (!priorityQueue.check()) {
//                            throw new AssertionError();
//                        }
                    }
                    if (fair) {
                        locks[mj].unlock();
                        locks[mi].unlock();
                    }
                }
            });
            workers[i].start();
        }

        for (Thread worker : workers) {
            try {
                worker.join();
            } catch (InterruptedException e) {
            }
        }

        System.out.println(String.format("Updates: %d", iterations.get()));

        return mrf.getNodeProbabilities();
    }
}
