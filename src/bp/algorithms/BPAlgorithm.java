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
}
