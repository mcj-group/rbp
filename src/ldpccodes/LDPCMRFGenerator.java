package ldpccodes;

import bp.MRF.MRF;

import java.util.ArrayList;
import java.util.Random;

/**
 * Created by vaksenov on 13.12.2019.
 */
public class LDPCMRFGenerator {
    public static MRF generateMRF(LDPCCode code, double e, int seed) {
        Random rnd = new Random(seed);
        int n = code.n;
        int k = code.k;
        int l = code.l;
        int m = code.m;
        ArrayList<Integer> permutation = code.permutation;

        int errors = 0;
        MRF mrf = new MRF(n + m);
        int[] y = new int[n];
        for (int i = 0; i < n; i++) {
            if (rnd.nextDouble() < e) {
                y[i] = 1;
                errors++;
            }
            double[] potential = new double[2];
            potential[y[i]] = 1 - e;
            potential[1 - y[i]] = e;
            mrf.setNodePotential(i, potential);
        }
        System.out.println("Number of errors: " + errors);

        for (int i = 0; i < m; i++) {
            double[] potential = new double[1 << l];
            for (int mask = 0; mask < potential.length; mask++) {
                if (Integer.bitCount(mask) % 2 == 0) {
                    potential[mask] = 1;
                }
            }
            mrf.setNodePotential(n + i, potential);
        }

        for (int i = 0; i < m; i++) {
            for (int j = 0; j < l; j++) {
                double[][] potential = new double[1 << l][2];
                for (int mask = 0; mask < 1 << l; mask++) {
                    potential[mask][(mask >> j) & 1] = 1;
                }
                mrf.addEdge(n + i, permutation.get(i * l + j) / k, potential);
            }
        }

        return mrf;
    }

}
