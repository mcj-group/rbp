package bp.algorithms;

import bp.MRF.Message;
import bp.MRF.MRF;
import bp.MRF.Utils;
import bp.algorithms.queues.SequentialPriorityQueue;

import java.util.Arrays;
import java.util.Collection;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class ResidualBP extends BPAlgorithm {
    double sensitivity;
    SequentialPriorityQueue<Message> priorityQueue;

    public ResidualBP(MRF mrf, double sensitivity) {
        super(mrf);
        this.sensitivity = sensitivity;
    }

    private double getPriority(Message e) {
        return Utils.distance(e.logMu, mrf.getFutureMessage(e));
    }

    public double[][] solve() {
        Collection<Message> messages = mrf.getMessages();
        priorityQueue = new SequentialPriorityQueue<>(messages.size());
        for (Message message : messages) {
            priorityQueue.insert(message, getPriority(message));
        }

        int it = 0;
        while (priorityQueue.peek().getValue() > sensitivity) {
            if (++it % 100000 == 0) {
                System.err.println(String.format("Iteration %d with maximal error %f", it,
                        priorityQueue.peek().getValue()));
            }
            System.err.println(priorityQueue.peek().getKey().i + " " + priorityQueue.peek().getKey().j + " " +
                    priorityQueue.peek().getValue());
//            System.err.println(Arrays.deepToString(mrf.getNodeProbabilities()));
            Message e = priorityQueue.peek().getKey();
            mrf.updateMessage(e, mrf.getFutureMessage(e));
            priorityQueue.changePriority(e, 0);
            for (Message affected : mrf.getMessagesFrom(e.j)) {
                priorityQueue.changePriority(affected, getPriority(affected));
            }
        }
        return mrf.getNodeProbabilities();
    }
}
