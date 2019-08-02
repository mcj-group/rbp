package bp.MRF;

import bp.algorithms.queues.IdentifiedClass;

import java.util.Arrays;

/**
 * Created by vaksenov on 23.07.2019.
 */
public class Message extends IdentifiedClass {
    public int i, j;
    Edge e;
    public double[] logMu;
    public Message reverse;

    public Message(int id, int i, int j, Edge e) {
        this.id = id;
        this.i = i;
        this.j = j;

        this.e = e;
        this.logMu = new double[e.potentials[0].length];
        Arrays.fill(logMu, Math.log(1. / logMu.length));
    }

    public double getPotential(int vi, int vj) {
        return e.getPotential(i, j, vi, vj);
    }
}
