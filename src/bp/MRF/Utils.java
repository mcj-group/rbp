package bp.MRF;

import java.util.Arrays;

/**
 * Created by vaksenov on 23.07.2019.
 */
public class Utils {
    public static double logSum(double[] logs) {
//        double maxLog = Arrays.stream(logs).max().getAsDouble();
        double maxLog = Double.NEGATIVE_INFINITY;
        for (double log : logs) {
            maxLog = Math.max(maxLog, log);
        }
        double sumExp = 0;
        for (double x : logs) {
            sumExp += Math.exp(x - maxLog);
        }
        return maxLog + Math.log(sumExp);
    }

    public static double distance(double[] log1, double[] log2) {
        double ans = 0;
        for (int i = 0; i < log1.length; i++) {
            ans += Math.abs(Math.exp(log1[i]) - Math.exp(log2[i]));
        }
        return ans;
    }
}
