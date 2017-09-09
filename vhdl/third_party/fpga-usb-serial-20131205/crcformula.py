"""Compute symbolic byte-update formulae for USB CRC-5 and CRC-16."""

# USB CRC-5 generator polynomial: X^5 + X^2 + 1
poly5 = ( 1,0,1,0,0 ) # lsb-first !!

# USB CRC-16 generator polynomial: X^16 + X^15 + X^2 + 1
poly16 = ( 1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1 ) # lsb-first !!


def xorsym(p, q):
    """Compute XOR of two terms."""
    z = { }
    for v in p:
        if v not in q:
            z[v] = 1
    for v in q:
        if v not in p:
            z[v] = 1
    return z


def updsymcrc(c, bit, poly):
    """Update symbolic CRC for one incoming bit term."""

    # Note: In this function, the crc buffer (c) may be longer than
    # the actual crc polynomial (poly). In this case the polynomial
    # and crc are aligned at the msb end, and the "extra" lsb bits
    # in the buffer are used for shifting in instead of zeroes.
    # We use this to derive the decomposed formula for CRC-5.

    # t = (incoming bit) xor (high bit of c)
    t = xorsym(bit, c[-1])

    # c = c shl 1
    c = [ { } ] + c[:-1]

    # c = c xor ((high bit of c) and poly)
    d = len(c) - len(poly)
    for i in range(len(poly)):
        if poly[i]:
            c[i+d] = xorsym(c[i+d], t)

    return c


def symword(prefix, n):
    """Return a symbolic word of length n."""
    return [ { (prefix, i): 1 } for i in range(n) ]


def crcformula(poly, newbits=8):
    """Return update formula for CRC with given polynomial."""

    # Start with old CRC value
    y = symword('c', len(poly))

    # For 8 incoming bits (lsb first)
    for i in range(newbits):
        newbit = { ('i', i): 1 }
        # Update CRC to account for new bit
        y = updsymcrc(y, newbit, poly)

    # Return formula for new CRC
    return y


def decomposedformula(poly, newbits=8):
    """Return two-stage update formula for CRC."""

    # Construct old CRC value
    c = symword('c', len(poly))

    # Construct incoming byte and reverse its bits (weird property of USB CRC)
    b = symword('b', newbits)
    b.reverse()

    # XOR old CRC with incoming (bit-reversed) byte; aligning at the msb side
    t = [ xorsym(c[len(c)-i], b[len(b)-i]) for i in range(min(len(c), len(b)), 0, -1) ]

    # Refer to this intermediate result
    y = symword('t', len(t))

    # Prepend any unused bits from the old CRC or from the incoming byte
    if len(c) > len(t): y = c[:len(c)-len(t)] + y
    if len(b) > len(t): y = b[:len(b)-len(t)] + y

    # Update word for each of the incoming bits
    for i in range(newbits):
        y = updsymcrc(y, { }, poly)

    # Truncate word to length of CRC
    if len(poly) < len(y):
        assert sum(map(len, y[:len(y)-len(poly)])) == 0
        y = y[len(y)-len(poly):]

    # Return the two formulas
    return t, y


def twicehalfformula(poly):
    """Return three-stage update formula for CRC."""

    # Construct old CRC value
    c = symword('c', len(poly))

    # Construct incoming byte and reverse its bits (weird property of USB CRC)
    b = symword('b', 8)
    b.reverse()

    # XOR old CRC with incoming (bit-reversed) byte; aligning at the msb side
    t = [ xorsym(c[len(c)-i], b[len(b)-i]) for i in range(min(len(c), len(b)), 0, -1) ]

    # Refer to this intermediate result
    x = symword('t', len(t))

    # Prepend any unused bits from the old CRC or from the incoming byte
    if len(c) > len(t): x = c[:len(c)-len(t)] + x
    if len(b) > len(t): x = b[:len(b)-len(t)] + x

    # Update word for 4 zero bits
    for i in range(4):
        x = updsymcrc(x, { }, poly)

    # Truncate word to needed length
    if len(poly) < len(x) and 4 < len(x):
        assert sum(map(len, x[:len(x)-max(len(poly),4)])) ==  0
        x = x[len(x)-max(len(poly),4):]

    # Refer to this intermediate result
    y = symword('x', len(x))

    # Update word for 4 zero bits
    for i in range(4):
        y = updsymcrc(y, { }, poly)

    # Truncate word to length of CRC
    if len(poly) < len(y):
        assert sum(map(len, y[:len(y)-len(poly)])) == 0
        y = y[len(y)-len(poly):]

    # Return the three formulas
    return t, x, y


def formatformula(y):
    """Print a formula in VHDL-like notation, msb-first."""
    f = [ ]
    for tdict in y:
        tlist = tdict.keys()
        tlist.sort()
        tstr = ' xor '.join([ '%s(%d)' % t for t in tlist ])
        if not tstr: tstr = '0'
        f.append(tstr)
    f.reverse()
    return '(\n    ' + ',\n    '.join(f) + ' )'


def makecrcformula(poly):
    polytmp = map(str, poly)
    polytmp.reverse()
    polystr = ''.join(polytmp) + 'B'
    del polytmp

    print "-- Plain formula for CRC-%d (poly %s)" % (len(poly), polystr)
    y = crcformula(poly)
    print "y := " + formatformula(y) + ";"
    print

    print "-- Decomposed formula for CRC-%d (poly %s)" % (len(poly), polystr)
    t, y = decomposedformula(poly)
    print "t := " + formatformula(t) + ";"
    print "y := " + formatformula(y) + ";"
    print


if __name__ == "__main__":
    makecrcformula(poly5)
    makecrcformula(poly16)

