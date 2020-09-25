import time
import random
import numpy as np


class Online:

    def __init__(self):
        self.K = 0
        self.n = 0
        self.Ex = 0
        self.Ex2 = 0

    def add_variable(self, x):
        if (self.n == 0):
            self.K = x
        self.n += 1
        self.Ex += x - self.K
        self.Ex2 += (x - self.K) * (x - self.K)

    def remove_variable(self, x):
        self.n -= 1
        self.Ex -= (x - self.K)
        self.Ex2 -= (x - self.K) * (x - self.K)

    def get_mean(self):
        return self.K + self.Ex / self.n

    def get_variance(self):
        return (self.Ex2 - (self.Ex * self.Ex) / self.n) / self.n


if __name__ == '__main__':

    TOTAL_NUM = 100
    cnt = 0
    data = [0] * TOTAL_NUM
    y = np.array([])


    online = Online()

    for i in range(200):

        val = random.random()

        s = time.time()
        if len(y) == TOTAL_NUM:
            y = np.delete(y, 0)
        y = np.append(y, val)
        print("Numpy : {:f}, {:f}, {:f}, time: {:.3f} ms"
              .format(y.mean(), y.std(), y.var(), (time.time() - s) * 1000))

        s = time.time()
        if cnt == TOTAL_NUM:
            cnt = 0

        if online.n == TOTAL_NUM:
            online.remove_variable(data[cnt])
        data[cnt] = val
        online.add_variable(data[cnt])

        average = online.get_mean()
        var = online.get_variance()

        print("Online: {:f}, {:f}, {:f}, time: {:.3f} ms"
              .format(average, np.sqrt(var), var, (time.time() - s) * 1000))

        cnt += 1
