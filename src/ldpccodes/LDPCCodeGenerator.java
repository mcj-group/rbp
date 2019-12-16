package ldpccodes;

import bp.MRF.ExamplesMRF;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.Random;

/**
 * Created by vaksenov on 13.12.2019.
 */
public class LDPCCodeGenerator {
    public static int gcd(int a, int b) {
        if (b == 0) {
            return a;
        } else {
            return gcd(b, a % b);
        }
    }

    public static LDPCCode generateLDPCCode(int n, int k, int l, int seed) {
        LDPCCode code = new LDPCCode();
        int g = gcd(k, l);
        n -= n % (l / g);
        int m = n * k / l;

        code.n = n;
        code.k = k;
        code.l = l;
        code.m = m;

        Random rnd = new Random(seed);
        ArrayList<Integer> permutation = new ArrayList<>(n * k);
        for (int i = 0; i < n * k; i++) {
            permutation.add(i);
        }
        boolean good = false;
        while (!good) {
            Collections.shuffle(permutation, rnd);
            good = true;
            for (int i = 0; i < m; i++) {
                HashSet<Integer> pairs = new HashSet<>();
                for (int j = 0; j < l; j++) {
                    pairs.add(permutation.get(i * l + j) / k);
                }
                if (pairs.size() != l) {
                    good = false;
                }
            }
        }

        code.permutation = permutation;

        return code;
    }
}
