package bp.algorithms;

import bp.MRF.Message;
import bp.MRF.MRF;
import bp.MRF.Utils;
import bp.algorithms.queues.*;

import java.util.Collection;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class SmartRelaxedPriorityBP extends BPAlgorithm {
    int threads;
    boolean fair;
    boolean relaxed;
    double sensitivity;

    public SmartRelaxedPriorityBP(MRF mrf, int threads, boolean fair, boolean relaxed, double sensitivity) {
        super(mrf);
        this.threads = threads;
        this.sensitivity = sensitivity;
        this.fair = fair;
        this.relaxed = relaxed;
    }

    private double getInitialPriority(Message m) {
        double v = Double.NEGATIVE_INFINITY;
        for (int vali = 0; vali < mrf.getNumberOfValues(m.i); vali++) {
            for (int valj = 0; valj < mrf.getNumberOfValues(m.j); valj++) {
                v = Math.max(v, 2 * Math.abs(Math.log(m.getPotential(vali, valj))));
            }
        }
        return v;
    }

    private double getResidual(Message m, double[] future) {
        double res = Double.NEGATIVE_INFINITY;
        for (int i = 0; i < future.length; i++) {
            res = Math.max(res, Math.abs(future[i] - m.logMu[i]));
        }
        return res;
    }

    public double[][] solve() {
        ReentrantLock[] locks = new ReentrantLock[mrf.getNodes()];
        for (int i = 0; i < locks.length; i++) {
            locks[i] = new ReentrantLock();
        }
        Collection<Message> messages = mrf.getMessages();
        final PQ<Message> priorityQueue = relaxed ?
                new MultiPQ<>(messages.size(), 4 * threads) :
                new ConcurrentPQ<>(messages.size());
        double[] currentPriority = new double[messages.size()];
        for (Message message : messages) {
            currentPriority[message.id] = getInitialPriority(message);
            priorityQueue.insert(message, currentPriority[message.id]);
//            priorityQueue.insert(message, getResidual(message, mrf.getFutureMessage(message)));
        }

        Thread[] workers = new Thread[threads];
        for (int i = 0; i < workers.length; i++) {
            workers[i] = new Thread(() -> {
                int it = 0;
                while (true) {
                    if (++it % 1000 == 0) {
                        if (priorityQueue.peek().priority < sensitivity) {
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

                    double[] futureM = mrf.getFutureMessage(m);

                    double r = getResidual(m, futureM);

                    mrf.updateMessage(m, futureM);

                    Collection<Message> messagesFromJ = mrf.getMessagesFrom(m.j);
                    for (Message affected : messagesFromJ) {
                        if (affected.j == m.i) {
                            continue;
                        }
                        currentPriority[affected.id] += r;
                        priorityQueue.changePriority(affected, currentPriority[affected.id]);
                    }

                    currentPriority[m.id] = r;
                    priorityQueue.insert(m, r);
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
