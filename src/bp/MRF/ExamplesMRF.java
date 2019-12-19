package bp.MRF;

import ldpccodes.LDPCCode;
import ldpccodes.LDPCCodeGenerator;
import ldpccodes.LDPCMRFGenerator;

import java.util.*;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class ExamplesMRF {
    public static MRF IsingMRF(int n, int C, int seed) {
        return IsingMRF(n, n, C, seed);
    }

    public static MRF IsingMRF(int n, int m, int C, int seed) {
        Random rnd = new Random(seed);
        MRF mrf = new MRF(n * m);

        for (int i = 0; i < n * m; i++) {
            double[] potential = new double[2];
            double beta = (rnd.nextDouble() - 0.5) * C;
            for (int j = 0; j < 2; j++) {
                int v = 2 * j - 1;
                potential[j] = rnd.nextDouble();//Math.exp(beta * v);
            }
            mrf.setNodePotential(i, potential);
        }

        int[] dx = new int[]{1, 0};
        int[] dy = new int[]{0, 1};

        for (int x = 0; x < n; x++) {
            for (int y = 0; y < m; y++) {
                int i = x * m + y;
                for (int k = 0; k < dx.length; k++) {
                    if (x + dx[k] < n && y + dy[k] < m) {
                        int j = (x + dx[k]) * m + (y + dy[k]);
                        double alpha = (rnd.nextDouble() - 0.5) * C;
                        double[][] potential = new double[2][2];
                        for (int a = 0; a < 2; a++) {
                            for (int b = 0; b < 2; b++) {
                                int vi = 2 * a - 1;
                                int vj = 2 * b - 1;
                                potential[a][b] = Math.exp(alpha * vi * vj);
                            }
                        }
                        mrf.addEdge(i, j, potential);
                    }
                }
            }
        }

        return mrf;
    }

    public static MRF PottsMRF(int n, int C, int seed) {
        Random rnd = new Random(seed);
        MRF mrf = new MRF(n * n);

        for (int i = 0; i < n * n; i++) {
            double[] potential = new double[2];
            double beta = (rnd.nextDouble() - 0.5) * C;
            potential[0] = 1;
            potential[1] = Math.exp(beta);
            mrf.setNodePotential(i, potential);
        }

        int[] dx = new int[]{1, 0};
        int[] dy = new int[]{0, 1};
        for (int x = 0; x < n; x++) {
            for (int y = 0; y < n; y++) {
                int i = x * n + y;
                for (int k = 0; k < dx.length; k++) {
                    if (x + dx[k] < n && y + dy[k] < n) {
                        int j = (x + dx[k]) * n + (y + dy[k]);
                        double alpha = (rnd.nextDouble() - 0.5) * C;
                        double[][] potential = new double[2][2];
                        for (int vali = 0; vali < 2; vali++) {
                            for (int valj = 0; valj < 2; valj++) {
                                if (vali == valj) {
                                    potential[vali][valj] = Math.exp(alpha);
                                } else {
                                    potential[vali][valj] = 1;
                                }
                            }
                        }
                        mrf.addEdge(i, j, potential);
                    }
                }
            }
        }
        return mrf;
    }

    public static MRF chain(int n, int C, int seed) {
        Random rnd = new Random(seed);
        MRF mrf = new MRF(n);
        for (int i = 0; i < n; i++) {
            double[] potential = new double[2];
            for (int j = 0; j < 2; j++) {
                potential[j] = rnd.nextDouble() * C;
            }
            mrf.setNodePotential(i, potential);
        }

        for (int i = 0; i < n; i++) {
            if (i + 1 < n) {
                int j = i + 1;
                double[][] potential = new double[2][2];
                for (int a = 0; a < 2; a++) {
                    for (int b = 0; b < 2; b++) {
                        potential[a][b] = rnd.nextDouble() * C; //Math.exp(alpha * vi * vj);
                    }
                }
                mrf.addEdge(i, j, potential);
            }
        }
        return mrf;
    }

    public static MRF deterministicChain(int n) {
        MRF mrf = new MRF(n);
        mrf.setNodePotential(0, new double[]{0, 1});
        for (int i = 1; i < n; i++) {
            mrf.setNodePotential(0, new double[]{0.5, 0.5});
        }

        for (int i = 0; i < n - 1; i++) {
            mrf.addEdge(i, i + 1, new double[][]{{0.25, 0.25}, {0.25, 0.25}});
        }
        return mrf;
    }

    public static MRF randomTree(int n, int C, int seed) {
        Random rnd = new Random(seed);
        MRF mrf = new MRF(n);

        for (int i = 0; i < n; i++) {
            double[] potential = new double[2];
            for (int j = 0; j < 2; j++) {
                potential[j] = rnd.nextDouble() * C;
            }
            mrf.setNodePotential(i, potential);
        }

        for (int i = 1; i < n; i++) {
            int p = rnd.nextInt(i);

            double[][] potential = new double[2][2];
            for (int a = 0; a < 2; a++) {
                for (int b = 0; b < 2; b++) {
                    potential[a][b] = rnd.nextDouble() * C;
                }
            }
            mrf.addEdge(i, p, potential);
        }
        return mrf;
    }

    public static MRF residualPaperExample() {
        MRF mrf = new MRF(4);

        for (int i = 0; i < 4; i++) {
            mrf.setNodePotential(i, new double[]{1, 1});
        }

        mrf.addEdge(0, 1, new double[][]{{.25, .25}, {.5, .25}});
        mrf.addEdge(1, 2, new double[][]{{1, 0.5}, {0.5, 0.5}});
        mrf.addEdge(2, 3, new double[][]{{1, .5}, {.5, 1}});
        return mrf;
    }

    /* LDPC code information */
    public static LDPCCode code;

    /* Length n, (k, l)-LDPC code */
    public static MRF LDPCCodes(int n, int k, int l, double e, int seed) {
        code = LDPCCodeGenerator.generateLDPCCode(n, k, l, seed);
        return LDPCMRFGenerator.generateMRF(code, e, seed);
    }

    public static boolean LDPCCodesCheckCorrectness(double[][] res) {
        int n = code.n;
        int k = code.k;
        int l = code.l;

        int m = (n * k) / l;

        ArrayList<Integer> permutation = code.permutation;

        int[] y = new int[n];
        for (int i = 0; i < y.length; i++) {
            if (res[i][0] < res[i][1]) {
                y[i] = 1;
            }
        }

        for (int i = 0; i < m; i++) {
            int sum = 0;
            for (int j = 0; j < l; j++) {
                int z = permutation.get(i * l + j) / k;
                sum += y[z];
            }
            if (sum % 2 == 1) {
                return false;
            }
        }
        return true;
    }
}
