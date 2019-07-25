package bp.algorithms;

import bp.MRF.MRF;

import java.util.Arrays;

/**
 * Created by vaksenov on 25.07.2019.
 */
public class BruteforceBP extends BPAlgorithm {
    public BruteforceBP(MRF mrf) {
        super(mrf);
    }

    double[][] sums;

    public void configurations(int i, int[] values) {
        if (i == values.length) {
            double energy = mrf.evaluateEnergy(values);
            for (int j = 0; j < values.length; j++) {
                sums[j][values[j]] += energy;
            }
            return;
        }
        for (int next = 0; next < mrf.getNumberOfValues(i); next++) {
            values[i] = next;
            configurations(i + 1, values);
        }
    }

    public double[][] solve() {
        sums = new double[mrf.getNodes()][];
        for (int i = 0; i < sums.length; i++) {
            sums[i] = new double[mrf.getNumberOfValues(i)];
        }

        int[] values = new int[mrf.getNodes()];
        configurations(0, values);

        double[][] answer = new double[sums.length][];
        for (int i = 0; i < answer.length; i++) {
            answer[i] = Arrays.copyOf(sums[i], sums[i].length);
            double sum = Arrays.stream(sums[i]).sum();
            for (int j = 0; j < answer[i].length; j++) {
                answer[i][j] /= sum;
            }
        }

        return answer;
    }
}
