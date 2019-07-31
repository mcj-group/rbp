package bp.testing;

import bp.MRF.ExamplesMRF;
import bp.MRF.MRF;
import bp.algorithms.*;

import java.io.FileNotFoundException;
import java.io.PrintWriter;
import java.util.Arrays;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class Main {
    public static void main(String[] args) {
        MRF mrf;
        int size = Integer.parseInt(args[2]);
        int threads = args.length < 3 ? 1 : Integer.parseInt(args[3]);
        switch (args[1]) {
            case "ising":
                mrf = ExamplesMRF.IsingMRF(size, 2, 1);
                break;
            case "potts":
                mrf = ExamplesMRF.PottsMRF(size, 5, 1);
                break;
            case "chain":
                mrf = ExamplesMRF.chain(size, 5, 1);
                break;
            case "tree":
                mrf = ExamplesMRF.randomTree(size, 5, 1);
                break;
            case "example":
                mrf = ExamplesMRF.residualPaperExample();
                break;
            default:
                throw new AssertionError(String.format("MRF %s is not supported", args[1]));
        }

        double sensitivity = 1e-5;

        BPAlgorithm algorithm;
        switch (args[0]) {
            case "residual":
                algorithm = new ResidualBP(mrf, sensitivity);
                break;
            case "synchronous":
                algorithm = new SynchronousBP(mrf, sensitivity);
                break;
            case "concurrent-unfair":
                algorithm = new ConcurrentResidualBP(mrf, threads, false, sensitivity);
                break;
            case "concurrent-fair":
                algorithm = new ConcurrentResidualBP(mrf, threads, true, sensitivity);
                break;
            case "relaxed-unfair":
                algorithm = new RelaxedResidualBP(mrf, threads, false, sensitivity);
                break;
            case "relaxed-fair":
                algorithm = new RelaxedResidualBP(mrf, threads, true, sensitivity);
                break;
            case "bruteforce":
                algorithm = new BruteforceBP(mrf);
                break;
            default:
                throw new AssertionError(String.format("Algorithm %s is not supported", args[0]));
        }

        long start = System.currentTimeMillis();
        double[][] res = algorithm.solve();
        long end = System.currentTimeMillis();
        try {
            PrintWriter out = new PrintWriter(String.format("results/%s-%d-%s-%d.txt", args[0], threads, args[1], size));
            out.println(Arrays.deepToString(res));
            out.close();
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        }
        System.out.println(String.format("Execution time: %d", end - start));
    }
}
