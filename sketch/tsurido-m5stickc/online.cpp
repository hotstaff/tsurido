#include "online.h"

Online::Online()
{

}

Online::~Online()
{

}

void Online::add_variable(int* x)
{
        if (_n == 0)
                _K = *x;
        _n += 1;
        _Ex += *x - _K;
        _Ex2 += (*x - _K) * (*x - _K);
}

void Online::remove_variable(int* x)
{
        _n -= 1;
        _Ex -= (*x - _K);
        _Ex2 -= (*x - _K) * (*x - _K);
}
double Online::get_mean(void)
{
        return _K + _Ex / _n;
}

double Online::get_variance(void)
{
        return (_Ex2 - (_Ex * _Ex) / _n) / _n;
}

void Online::get_stat(int* x, double* mean, double* std)
{
        if(_pos == ONLINE_BUFFER_SIZE)
                _pos = 0;

        if(_n == ONLINE_BUFFER_SIZE)
                remove_variable(&_data[_pos]);

        _data[_pos] = *x;
        add_variable(&_data[_pos]);

        *mean = get_mean();
        *std = sqrt(get_variance());

        _pos++;
}

Online OL;
