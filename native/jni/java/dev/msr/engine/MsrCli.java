package dev.msr.engine;

/** Headless end-to-end check of the same API the Android app calls. */
public final class MsrCli {
    public static void main(String[] args) throws Exception {
        if (args.length < 2) {
            System.err.println("usage: MsrCli <model.gguf> <prompt> [maxTokens] [budgetMb] " +
                "[threads] [readThreads] [prefetch] [--cancel-after-tokens N|--cancel-after-ms M]");
            System.exit(2);
        }
        final String model = args[0];
        final String prompt = args[1];
        final int maxTokens = args.length > 2 ? Integer.parseInt(args[2]) : 8;
        final long budgetMb = args.length > 3 ? Long.parseLong(args[3]) : 1024;
        final int threads = args.length > 4 ? Integer.parseInt(args[4]) : 4;
        final int readThreads = args.length > 5 ? Integer.parseInt(args[5]) : 4;
        final boolean prefetch = args.length < 6 || !"0".equals(args[6]);

        final int cancelTokens = intFlag(args, "--cancel-after-tokens", 0);
        final int cancelMs = intFlag(args, "--cancel-after-ms", 0);

        final MsrEngine engine = new MsrEngine();
        try {
            engine.load(model, budgetMb << 20, 2048, threads, prefetch, readThreads);
            System.err.println("loaded: " + engine.stats());

            if (cancelMs > 0) {
                Thread canceller = new Thread(() -> {
                    try {
                        Thread.sleep(cancelMs);
                    } catch (InterruptedException ignored) {
                        return;
                    }
                    System.err.println("cancelling from another thread after " + cancelMs + " ms");
                    engine.cancel();
                });
                canceller.setDaemon(true);
                canceller.start();
            }

            final int[] emitted = {0};
            engine.generate(prompt, maxTokens, 0.0f, 42, piece -> {
                System.out.print(piece);
                System.out.flush();
                emitted[0]++;
                return cancelTokens == 0 || emitted[0] < cancelTokens;
            });
            System.out.println();
            if (cancelTokens > 0) {
                System.err.println("cancelled via sink after " + emitted[0] + " tokens");
            }
            System.err.println(engine.stats());
        } finally {
            engine.close();
        }
    }

    private static int intFlag(String[] args, String name, int fallback) {
        for (int i = 0; i < args.length - 1; i++) {
            if (name.equals(args[i])) return Integer.parseInt(args[i + 1]);
        }
        return fallback;
    }
}
