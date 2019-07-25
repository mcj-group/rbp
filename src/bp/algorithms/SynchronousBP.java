package bp.algorithms;

import bp.MRF.Message;
import bp.MRF.MRF;
import bp.MRF.Utils;

import java.util.Collection;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class SynchronousBP  extends BPAlgorithm{
    double sensitivity;

    public SynchronousBP(MRF mrf, double sensitivity) {
        super(mrf);
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
        Collection<Message> messages = mrf.getMessages();
        double[][] new_mu = new double[messages.size()][];

        int it = 0;
        boolean updated = true;
        while (updated) {
            if (++it % 10000 == 0) {
                System.err.println(String.format("Current error: %f", totalError(messages)));
            }
            updated = false;
            for (Message message : messages) {
                new_mu[message.id] = mrf.getFutureMessage(message);
            }
            for (Message message : messages) {
                if (Utils.distance(message.logMu, new_mu[message.id]) > sensitivity) {
                    updated = true;
                    mrf.updateMessage(message, new_mu[message.id]);
                }
            }
        }
        return mrf.getNodeProbabilities();
    }
}
