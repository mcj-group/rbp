package bp.testing;

import bp.MRF.ExamplesMRF;
import bp.MRF.MRF;
import bp.algorithms.*;

import java.util.Arrays;

/**
 * Created by vaksenov on 24.07.2019.
 */
public class Main {
    public static void main(String[] args) {
        MRF mrf;
        switch (args[1]) {
            case "ising":
                mrf = ExamplesMRF.IsingMRF(20, 11, 1);
                break;
            case "potts":
                mrf = ExamplesMRF.PottsMRF(100, 5, 1);
                break;
            case "chain":
                mrf = ExamplesMRF.chain(20, 5, 1);
                break;
            case "tree":
                mrf = ExamplesMRF.randomTree(10, 5, 1);
                break;
            default:
                throw new AssertionError(String.format("MRF %s is not supported", args[1]));
        }

        double sensitivity = 1e-10;

        BPAlgorithm algorithm;
        switch (args[0]) {
            case "residual":
                algorithm = new ResidualBP(mrf, sensitivity);
                break;
            case "synchronous":
                algorithm = new SynchronousBP(mrf, sensitivity);
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
        System.err.println(Arrays.deepToString(res));
        System.out.println(String.format("Execution time: %d", end - start));
    }
}
