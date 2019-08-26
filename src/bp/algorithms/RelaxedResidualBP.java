package bp.algorithms;

import bp.MRF.Message;
import bp.MRF.MRF;
import bp.MRF.Utils;
import bp.algorithms.queues.MultiPQ;

import java.util.Collection;
import java.util.concurrent.locks.ReentrantLock;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class RelaxedResidualBP extends BPAlgorithm {
    int threads;
    boolean fair;
    double sensitivity;

    public RelaxedResidualBP(MRF mrf, int threads, boolean fair, double sensitivity) {
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
        final MultiPQ<Message> priorityQueue = new MultiPQ<>(messages.size(), 4 * threads);
        for (Message message : messages) {
            priorityQueue.insert(message, getPriority(message));
        }
        Thread[] workers = new Thread[threads];
        AtomicInteger iterations = new AtomicInteger();
        for (int i = 0; i < workers.length; i++) {
            workers[i] = new Thread(() -> {
                int it = 0;
                while (true) {
                    if (++it % 1000 == 0) {
//                        System.err.println(it);
//                        System.err.println(priorityQueue.peek().getValue());
                        if (priorityQueue.peek().priority < sensitivity) {
                            iterations.addAndGet(it);
                            return;
                        }
                    }
                    Message m = priorityQueue.extractMin();

                    int mi = Math.min(m.i, m.j);
                    int mj = Math.max(m.i, m.j);
                    if (fair) {
                        locks[mi].lock();
                        locks[mj].lock();
                    }
                    mrf.updateMessage(m);

                    Collection<Message> messagesFromJ = mrf.getMessagesFrom(m.j);
                    for (Message affected : messagesFromJ) {
                        if (affected.j != m.i) {
                            priorityQueue.changePriority(affected, getPriority(affected));
                        }
                    }

                    priorityQueue.insert(m, 0);
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

        System.out.println(String.format("Iterations to convergence: %d", iterations.get()));

        return mrf.getNodeProbabilities();
    }
}
