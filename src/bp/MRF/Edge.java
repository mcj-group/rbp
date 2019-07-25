package bp.MRF;

/**
 * Created by vaksenov on 25.07.2019.
 */
public class Edge {
    public int u, v;
    public double[][] potentials;

    public Edge(int u, int v, double[][] potentials) {
        this.u = u;
        this.v = v;
        this.potentials = potentials;
    }

    public double getPotential(int i, int j, int vi, int vj) {
        if (u == i) {
            return potentials[vi][vj];
        } else {
            return potentials[vj][vi];
        }
    }
}
