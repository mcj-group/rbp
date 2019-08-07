package bp.algorithms;

import bp.MRF.Message;
import bp.MRF.MRF;
import bp.MRF.Utils;
import bp.algorithms.queues.MultiPQ;
import bp.algorithms.queues.weighted.WeightedMultiPQ;
import bp.algorithms.queues.weighted.WeightedPriorityNode;

import java.util.Collection;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class WeightDecayBP extends BPAlgorithm {
    int threads;
    boolean fair;
    double sensitivity;

    public WeightDecayBP(MRF mrf, int threads, boolean fair, double sensitivity) {
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
        final WeightedMultiPQ<Message> priorityQueue = new WeightedMultiPQ<>(messages.size(), 4 * threads);
        for (Message message : messages) {
            double priority = getPriority(message);
            priorityQueue.insert(message, priority, priority);
        }
        Thread[] workers = new Thread[threads];
        for (int i = 0; i < workers.length; i++) {
            workers[i] = new Thread(() -> {
                int it = 0;
                while (true) {
                    if (++it % 1000 == 0) {
//                        System.err.println(it);
//                        System.err.println(priorityQueue.peek().getValue());
                        if (priorityQueue.peek().maxWeight < sensitivity) {
                            return;
                        }
                    }
                    WeightedPriorityNode<Message> node = priorityQueue.extractMin();

                    Message m = node.value;
                    int mi = Math.min(m.i, m.j);
                    int mj = Math.max(m.i, m.j);
                    if (fair) {
                        locks[mi].lock();
                        locks[mj].lock();
                    }

                    mrf.updateMessage(m);
                    node.totalUpdates++;

                    Collection<Message> messagesFromJ = mrf.getMessagesFrom(m.j);
                    for (Message affected : messagesFromJ) {
                        if (affected.j != m.i) {
                            priorityQueue.changePriority(affected, getPriority(affected));
                        }
                    }

                    priorityQueue.insert(m, 0, 0);

//                    if (!priorityQueue.check()) {
//                        throw new AssertionError();
//                    }

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

        return mrf.getNodeProbabilities();
    }
}
