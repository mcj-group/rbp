package ldpccodes;

import bp.MRF.MRF;
import bp.algorithms.*;

import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.Random;

/**
 * Created by vaksenov on 13.12.2019.
 */
public class Main {
    public static double[][] run(MRF mrf) {
        BPAlgorithm algorithm = BPAlgorithm.getAlgorithm("synchronous", mrf, 1e-2, 2);
        return algorithm.solve();
    }

    public static int run(LDPCCode code, double eps, int it, int seed) {
        System.err.println(String.format("Working for length %d and eps %f", code.n,  eps));
        int correct = 0;
        for (int i = 0; i < it; i++) {
            MRF mrf = LDPCMRFGenerator.generateMRF(code, eps, seed + i);
            long start = System.nanoTime();
            double[][] res = run(mrf);
            long time = System.nanoTime() - start;

            boolean ok = true;
            for (int j = 0; j < code.n; j++) {
                ok &= res[j][0] > res[j][1];
            }

            correct += ok ? 1 : 0;
            System.err.println(String.format("Iteration %d with %d corrects in time %fs", i + 1, correct, 1. * time / 1_000_000_000));
        }
        return correct;
    }

    public static void run(int n, double[] eps, int it, int seed) {
        try {
            File directory = new File("ldpc");
            if (!directory.exists()) {
                directory.mkdir();
            }
            PrintWriter out = new PrintWriter(String.format("ldpc/%d", n));
            Random rnd = new Random(seed);
            System.err.println(String.format("Working for length %d", n));
            LDPCCode code = LDPCCodeGenerator.generateLDPCCode(n, 3, 6, rnd.nextInt());
            for (double e : eps) {
                int correct = run(code, e, it, rnd.nextInt());
                out.println(e + " " + 1. * correct / it);
            }
            out.close();
        } catch (IOException e) {
        }
    }

    public static void main(String[] args) {
//        double[] eps = new double[]{0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.07, 0.08};
        double[] eps = new double[]{0.05};

        for (int n : new int[] {1000}) {
            run(n, eps, 10000, 1);
        }
    }
}
