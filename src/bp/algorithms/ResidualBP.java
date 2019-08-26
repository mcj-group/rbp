package bp.algorithms;

import bp.MRF.Message;
import bp.MRF.MRF;
import bp.MRF.Utils;
import bp.algorithms.queues.SequentialPQ;

import java.util.Collection;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class ResidualBP extends BPAlgorithm {
    double sensitivity;
    SequentialPQ<Message> priorityQueue;

    public ResidualBP(MRF mrf, double sensitivity) {
        super(mrf);
        this.sensitivity = sensitivity;
    }

    private double getPriority(Message e) {
        return Utils.distance(e.logMu, mrf.getFutureMessage(e));
    }

    public double[][] solve() {
        Collection<Message> messages = mrf.getMessages();
        priorityQueue = new SequentialPQ<>(messages.size());
        for (Message message : messages) {
            priorityQueue.insert(message, getPriority(message));
        }

        int it = 0;
        while (priorityQueue.peek().priority > sensitivity) {
            if (++it % 100000 == 0) {
                System.err.println(String.format("Iteration %d with maximal error %f", it,
                        priorityQueue.peek().priority));
            }
            Message m = priorityQueue.peek().value;

//            System.err.println(m.i + " " + m.j + " " + priorityQueue.peek().getValue());
//            System.err.println(Arrays.toString(m.logMu) + " " + Arrays.toString(mrf.getFutureMessage(m)) + " " +
//                Utils.distance(m.logMu, mrf.getFutureMessage(m)));
//            System.err.println(Arrays.deepToString(mrf.getNodeProbabilities()));

            mrf.updateMessage(m, mrf.getFutureMessage(m));
            priorityQueue.changePriority(m, 0);
            for (Message affected : mrf.getMessagesFrom(m.j)) {
                priorityQueue.changePriority(affected, getPriority(affected));
            }
        }
        System.out.println(String.format("Iterations to convergence: %d", it));
        return mrf.getNodeProbabilities();
    }
}
