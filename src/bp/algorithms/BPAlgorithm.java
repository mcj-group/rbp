package bp.algorithms;

import bp.MRF.MRF;

/**
 * Created by vaksenov on 24.07.2019.
 */
public abstract class BPAlgorithm {
    protected MRF mrf;

    public BPAlgorithm(MRF mrf) {
        this.mrf = mrf;
    }

    public abstract double[][] solve();

    public static BPAlgorithm getAlgorithm(String algorithm, MRF mrf, double sensitivity, int threads, String... args) {
        switch (algorithm) {
            case "residual":
                return new ResidualBP(mrf, sensitivity);
            case "synchronous":
                return new SynchronousBP(mrf, threads, sensitivity);
            case "concurrent-unfair":
                return new ConcurrentResidualBP(mrf, threads, false, sensitivity);
            case "concurrent-fair":
                return new ConcurrentResidualBP(mrf, threads, true, sensitivity);
            case "relaxed-unfair":
                return new RelaxedResidualBP(mrf, threads, false, sensitivity);
            case "relaxed-fair":
                return new RelaxedResidualBP(mrf, threads, true, sensitivity);
            case "splash-unfair":
                return new SplashBP(mrf, Integer.parseInt(args[0]), threads, false, false, sensitivity);
            case "splash-fair":
                return new SplashBP(mrf, Integer.parseInt(args[0]), threads, true, false, sensitivity);
            case "smart-splash-unfair":
                return new SmartSplashBP(mrf, Integer.parseInt(args[0]), threads, false, false, sensitivity);
            case "smart-splash-fair":
                return new SmartSplashBP(mrf, Integer.parseInt(args[0]), threads, true, false, sensitivity);
            case "relaxed-splash-unfair":
                return new SplashBP(mrf, Integer.parseInt(args[0]), threads, false, true, sensitivity);
            case "relaxed-splash-fair":
                return new SplashBP(mrf, Integer.parseInt(args[0]), threads, true, true, sensitivity);
            case "relaxed-smart-splash-unfair":
                return new SmartSplashBP(mrf, Integer.parseInt(args[0]), threads, false, true, sensitivity);
            case "relaxed-smart-splash-fair":
                return new SmartSplashBP(mrf, Integer.parseInt(args[0]), threads, true, true, sensitivity);
            case "relaxed-priority-fair":
                return new RelaxedPriorityBP(mrf, threads, true, true, sensitivity);
            case "smart-relaxed-priority-fair":
                return new SmartRelaxedPriorityBP(mrf, threads, true, true, sensitivity);
            case "weight-decay-fair":
                return new WeightDecayBP(mrf, threads, true, sensitivity);
            case "slow-weight-decay":
                return new SlowWeightDecayBP(mrf, threads, true, sensitivity);
            case "bruteforce":
                return new BruteforceBP(mrf);
            default:
                throw new AssertionError(String.format("Algorithm %s is not supported", algorithm));
        }
    }
}
