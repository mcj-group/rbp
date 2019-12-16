package bp.MRF;

/**
 * Created by vaksenov on 25.07.2019.
 */
public class Edge {
    public int u, v;
    public double[][] potentials;
    public double[][] logPotentials;

    public Edge(int u, int v, double[][] potentials) {
        this.u = u;
        this.v = v;
        this.potentials = potentials;
        this.logPotentials = new double[potentials.length][potentials[0].length];
        for (int i = 0; i < potentials.length; i++) {
            for (int j = 0; j < potentials[i].length; j++) {
                logPotentials[i][j] = Math.log(potentials[i][j]);
            }
        }
    }

    public double getPotential(int i, int j, int vi, int vj) {
        if (u == i) {
            return potentials[vi][vj];
        } else {
            return potentials[vj][vi];
        }
    }

    public double getLogPotential(int i, int j, int vi, int vj) {
        if (u == i) {
            return logPotentials[vi][vj];
        } else {
            return logPotentials[vj][vi];
        }
    }
}
