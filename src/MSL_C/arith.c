#include <climits>
#include <cstdlib>

int abs(int n) {
    if (n < 0) {
        return -n;
    }
    else {
        return n;
    }
}

div_t div(int numerator, int denominator) {
    int numSign;
    int denSign;
    div_t result;

    numSign = 1;
    denSign = 1;

    if (numerator < 0) {
        numerator = -numerator;
        numSign = -1;
    }

    if (denominator < 0) {
        denominator = -denominator;
        denSign = -1;
    }

    result.quot = (numerator / denominator) * (numSign * denSign);
    result.rem = numerator * numSign - denSign * (result.quot * denominator);

    return result;
}

// Adds addend to *sum unless the result would overflow. Returns 1 on success, 0 on overflow
int __msl_add(int* sum, int addend) {
    int value = *sum;

    if (addend < 0) {
        if (value < 0 && addend < INT_MIN - value) {
            return 0;
        }
    }
    else if (value > 0 && addend > INT_MAX - value) {
        return 0;
    }

    *sum = value + addend;
    return 1;
}

// long version of __msl_add
int __msl_ladd(long* sum, long addend) {
    long value = *sum;

    if (addend < 0) {
        if (value < 0 && addend < LONG_MIN - value) {
            return 0;
        }
    }
    else if (value > 0 && addend > LONG_MAX - value) {
        return 0;
    }

    *sum = value + addend;
    return 1;
}

// Multiplies *product by factor unless the result would overflow. Returns 1 on success, 0 on overflow
int __msl_mul(int* product, int factor) {
    int sign = 1;
    int value = *product;

    if ((value < 0) ^ (factor < 0)) {
        sign = -1;
    }

    if (value < 0) {
        value = -value;
    }

    if (factor < 0) {
        factor = -factor;
    }

    if (value > INT_MAX / factor) {
        return 0;
    }

    *product = sign * (value * factor);
    return 1;
}

// Floor division: the quotient rounds toward negative infinity and the remainder takes the divisor's sign
div_t __msl_div(int numerator, int denominator) {
    int rem;
    int quot;
    int numSign = 1;
    int denSign = 1;
    int sign;
    div_t result;

    if (numerator < 0) {
        numerator = -numerator;
        numSign = -1;
    }

    if (denominator < 0) {
        denominator = -denominator;
        denSign = -1;
    }

    sign = numSign * denSign;
    quot = sign * (numerator / denominator);
    rem = numerator * numSign - denSign * (quot * denominator);

    if (rem != 0 && sign < 0) {
        quot--;
        rem += denominator * denSign;
    }

    result.quot = quot;
    result.rem = rem;
    return result;
}

// Remainder of floor division (takes the divisor's sign)
int __msl_mod(int numerator, int denominator) {
    int rem;
    int quot;
    int numSign = 1;
    int denSign = 1;
    int sign;

    if (numerator < 0) {
        numerator = -numerator;
        numSign = -1;
    }

    if (denominator < 0) {
        denominator = -denominator;
        denSign = -1;
    }

    sign = numSign * denSign;
    quot = sign * (numerator / denominator);
    rem = numerator * numSign - denSign * (quot * denominator);

    if (rem != 0 && sign < 0) {
        rem += denominator * denSign;
    }

    return rem;
}
