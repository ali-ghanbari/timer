package org.mudebug;

public class Main {
    private static void fibo(int n) throws Exception {
        Thread.sleep(n);
    }

    public static void main(String[] args) throws Exception {
        fibo(0);
	fibo(1);
	fibo(600);
    }
}
