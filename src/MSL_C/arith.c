#include <cstdlib>

#define INT_MAX 0x7FFFFFFF
#define INT_MIN (-INT_MAX - 1)
#define LONG_MAX 0x7FFFFFFFL
#define LONG_MIN (-LONG_MAX - 1)

typedef struct {
    int quot;
    int rem;
} div_t;

int abs(int n) {
    if (n < 0) {
        return -n;
    }
    else {
        return n;
    }
}

div_t div(int numerator, int denominator) {
    int n_sign, d_sign;
    div_t value;

    n_sign = 1;
    d_sign = 1;

    if (numerator < 0) {
        numerator = -numerator;
        n_sign = -1;
    }

    if (denominator < 0) {
        denominator = -denominator;
        d_sign = -1;
    }

    value.quot = (numerator / denominator) * (n_sign * d_sign);
    value.rem = (numerator * n_sign) - (value.quot * denominator * d_sign);

    return value;
}

// Adds y to *x unless the sum overflows, returns whether it did
int __msl_add(int* x, int y) {
    int z = *x;

    if (y < 0) {
        if (z < 0 && y < INT_MIN - z) {
            return 0;
        }
    }
    else if (z > 0 && y > INT_MAX - z) {
        return 0;
    }

    *x = z + y;
    return 1;
}

int __msl_ladd(long* x, long y) {
    long z = *x;

    if (y < 0) {
        if (z < 0 && y < LONG_MIN - z) {
            return 0;
        }
    }
    else if (z > 0 && y > LONG_MAX - z) {
        return 0;
    }

    *x = z + y;
    return 1;
}

// Multiplies *x by y unless the product overflows, returns whether it did
int __msl_mul(int* x, int y) {
    int sign = 1;
    int z = *x;

    if ((z < 0) ^ (y < 0)) {
        sign = -1;
    }

    if (z < 0) {
        z = -z;
    }

    if (y < 0) {
        y = -y;
    }

    if (z > INT_MAX / y) {
        return 0;
    }

    *x = sign * (z * y);
    return 1;
}

// Division that rounds toward negative infinity
div_t __msl_div(int x, int y) {
    int rem, quot, sign_x, sign_y, sign;
    div_t value;

    sign_x = 1;
    sign_y = 1;

    if (x < 0) {
        x = -x;
        sign_x = -1;
    }

    if (y < 0) {
        y = -y;
        sign_y = -1;
    }

    sign = sign_x * sign_y;
    quot = sign * (x / y);
    rem = (x * sign_x) - (sign_y * (quot * y));

    if (rem != 0 && sign < 0) {
        quot--;
        rem += y * sign_y;
    }

    value.quot = quot;
    value.rem = rem;
    return value;
}

// Remainder with the sign of the divisor
int __msl_mod(int x, int y) {
    int rem, quot, sign_x, sign_y, sign;

    sign_x = 1;
    sign_y = 1;

    if (x < 0) {
        x = -x;
        sign_x = -1;
    }

    if (y < 0) {
        y = -y;
        sign_y = -1;
    }

    sign = sign_x * sign_y;
    quot = sign * (x / y);
    rem = (x * sign_x) - (sign_y * (quot * y));

    if (rem != 0 && sign < 0) {
        rem += y * sign_y;
    }

    return rem;
}
