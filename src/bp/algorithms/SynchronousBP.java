package bp.algorithms;

import bp.MRF.Message;
import bp.MRF.MRF;
import bp.MRF.Utils;

import java.util.ArrayList;
import java.util.Collection;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class SynchronousBP extends BPAlgorithm {
    double sensitivity;
    int threads;

    public SynchronousBP(MRF mrf, int threads, double sensitivity) {
        super(mrf);
        this.threads = threads;
        this.sensitivity = sensitivity;
    }

    public double totalError(Collection<Message> messages) {
        double result = 0;
        for (Message message : messages) {
            result += Utils.distance(message.logMu, mrf.getFutureMessage(message));
        }
        return result;
    }

    public double[][] solve() {
        final ArrayList<Message> messages = new ArrayList<>(mrf.getMessages());
        double[][] new_mu = new double[messages.size()][];

        int it = 0;
        int updates = 0;
        boolean updated = true;
        while (updated) {
//            System.err.println(it);
//            if (it == 20) {
//                break;
//            }
            if (++it % 10000 == 0) {
                System.err.println(String.format("Current error: %f", totalError(messages)));
            }
            updated = false;
            Thread[] threads = new Thread[this.threads];
            final int len = (messages.size() + this.threads - 1) / this.threads;
            for (int i = 0; i < this.threads; i++) {
                int id = i;
                threads[i] = new Thread(() -> {
                    for (int j = len * id; j < Math.min(len * (id + 1), messages.size()); j++) {
                        Message message = messages.get(j);
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

            final boolean[] updatedThread = new boolean[this.threads];
            final int[] updatesThread = new int[this.threads];
            for (int i = 0; i < this.threads; i++) {
                int id = i;
                threads[i] = new Thread(() -> {
                    boolean updatedLocal = false;
                    int updatesLocal = 0;
                    for (int j = len * id; j < Math.min(len * (id + 1), messages.size()); j++) {
                        Message message = messages.get(j);
                        if (Utils.distance(message.logMu, new_mu[message.id]) > sensitivity) {
                            updatedLocal = true;
                        }
                        updatesLocal++;
                        mrf.copyMessage(message, new_mu[message.id]);
                    }
                    updatedThread[id] = updatedLocal;
                    updatesThread[id] = updatesLocal;
                });
                threads[i].start();
            }

            for (int i = 0; i < this.threads; i++) {
                try {
                    threads[i].join();
                    updated |= updatedThread[i];
                    updates += updatesThread[i];
                } catch (InterruptedException e) {
                }
            }

            int lenNodes = (mrf.getNodes() + this.threads - 1) / this.threads;
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
            }

//            for (Message message : messages) {
//                new_mu[message.id] = mrf.getFutureMessage(message);
//            }
//
//            for (Message message : messages) {
//                if (Utils.distance(message.logMu, new_mu[message.id]) > sensitivity) {
//                    updated = true;
//                    updates++;
//                }
//                mrf.updateMessage(message, new_mu[message.id]);
//            }
        }
        System.out.println(String.format("Updates: %d", updates));
        return mrf.getNodeProbabilities();
    }
}
