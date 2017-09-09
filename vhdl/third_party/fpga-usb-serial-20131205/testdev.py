#!/usr/bin/python
"""Test USB device.

Usage: testdev.py /dev/ttyUSBn
"""

import sys, time, os, signal, atexit, serial


def commandBytes((d, c, v)):
    """Convert a test command to a sequence of bytes."""
    assert c > 0 and d < 256 and c < 256
    if d > 0:
        return chr(0) + chr(d) + chr(c) + chr(v)
    else:
        return chr(c) + chr(v)


def writeBackground(dev, s):
    """Fork a background process and write all given bytes."""
    pid = os.fork()
    if pid == 0:
        dev.setWriteTimeout(1000000)
        dev.write(s)
        os._exit(0)
    def killChild():
        os.kill(pid, signal.SIGKILL)
    atexit.register(killChild)


def runTest(dev, fastwrite, fastread):

    def fail(s):
        print >>sys.stderr, "\nFailed:", s
        sys.exit(1)

    if not fastread:
        # In read-throttled mode, we use very short sleeps in the device
        # to ensure that it overflows the host receive channel.
        delayseq = [ 0, 1, 1, 1, 1, 1, 1, 1 ]
    elif fastwrite:
        # In fast-write mode, we need at least a short sleep in the
        # the device to avoid overflowing the host receive buffer.
        delayseq = [ 10, 11, 12, 16, 21, 26, 31, 41 ]
    else:
        # Step-by-step mode.
        delayseq = [ 0, 1, 11, 16, 21, 26, 31, 41 ]

    # Generate a sequence of test commands.
    commandSequence = [ ]
    v = 1
    for d in delayseq:
        for c in range(1, 256):
            commandSequence.append( (d, c, v) )
            v = (v + 3) % 256
        v = (v + 7) % 256

    if fastwrite:
        # write test commands in background
        s = ''.join(map(commandBytes, commandSequence))
        writeBackground(dev, s)

    # loop over commands in test sequence
    i = 0
    for cmd in commandSequence:

        i += 1
        if i % 10 == 0:
            istr = " (%d)" % i
            sys.stderr.write(istr + (len(istr) * '\b'))
            sys.stderr.flush()

        # decode command
        (d, c, v) = cmd

        if not fastwrite:
            # write test commands one-by-one
            time.sleep(0.1)
            dev.write(commandBytes(cmd))

        # read response
        delay = d / 60.0
        if fastread:
            # read with timeout
            dev.setTimeout(delay + 0.1)
        else:
            # sleep before each read
            time.sleep(delay + 0.1)
            dev.setTimeout(0.1)
        t = time.time()
        rbuf = dev.read(c)
        t = time.time() - t

        # verify response
        if fastread and t < delay - 0.1:
            fail("early response to (d=%d, c=%d, v=%d)" % cmd)
        if not rbuf:
            fail("no response to (d=%d, c=%d, v=%d)" % cmd)
        for j in range(len(rbuf)):
            if ord(rbuf[j]) != (v + j) % 256:
                fail("invalid response to (d=%d, c=%d, v=%d) response=%s" % (d, c, v, repr(map(ord, rbuf))))
        if len(rbuf) != c:
            fail("short response to (d=%d, c=%d, v=%d) response=%s" % (d, c, v, repr(map(ord, rbuf))))

    # listen for spurious data
    dev.setTimeout(5.0)
    rbuf = dev.read()
    if rbuf:
        fail("spurious data at end of test sequence response=%s..." % repr(map(ord, rbuf)))
 
    print >>sys.stderr, " ok.    "


def testdev(devname):

    print >>sys.stderr, "Opening device '%s' ..." % devname,
    dev = serial.Serial(port=devname)
    print >>sys.stderr, "ok."

    print >>sys.stderr, "Emptying device buffers ...",
    dev.setTimeout(2.0)
    rcnt = 0
    rbuf = dev.read(4096)
    while rbuf:
        rcnt += len(rbuf)
        rbuf = dev.read(4096)
    print >>sys.stderr, "flushed %d bytes" % rcnt

    print >>sys.stderr, "Switching to text mode ...",
    dev.write('\x00\x00\x00\x05\r')
    time.sleep(1)
    rbuf = dev.read(4096)
    rcnt = len(rbuf)
    print >>sys.stderr, "flushed %d bytes" % rcnt

    # Just send a string; it should come back reversed.
    print >>sys.stderr, "Testing simple text string ...",
    dev.setTimeout(0.1)
    dev.write('Hello fpga.\r')
    time.sleep(1)
    rbuf = dev.read(4096)
    if rbuf == '.agpf olleH\r\n':
        print >>sys.stderr, " ok."
    else:
        print "\nFailed: got", repr(rbuf)
        sys.exit(1)

    # Test partial read of available data; rest of data should stay in buffer.
    print >>sys.stderr, "Testing partial read ...",
    dev.setTimeout(0.1)
    dev.write('Hello again.\r')
    time.sleep(1)
    rbuf = dev.read(6)
    rbuf2 = dev.read(8)
    if rbuf == '.niaga' and rbuf2 == ' olleH\r\n':
        print >>sys.stderr, " ok."
    else:
        print "\nFailed: got", repr(rbuf), "and", repr(rbuf2)
        sys.exit(1)

    # Enable TXCORK and send a string; nothing should happen until
    # TXCORK is disabled.
    print >>sys.stderr, "Testing TXCORK ...",
    dev.setTimeout(1)
    dev.write('One string of bytes in the buffer.\x04\r')
    rbuf = dev.read(4096)
    if rbuf:
        print >>sys.stderr, "\nFailed: expected nothing but got", repr(rbuf)
        sys.exit(1)
    dev.write('Two strings of bytes in the buffer.\r\x05')
    rbuf = dev.read(20) + dev.read(4096)
    if rbuf == '.reffub eht ni setyb fo gnirts enO\r\n.reffub eht ni setyb fo sgnirts owT\r\n':
        print >>sys.stderr, " ok."
    else:
        print "\nFailed: got", repr(rbuf)
        sys.exit(1)

    print >>sys.stderr, "Switching to binary mode ...\n",
    dev.write('\x01')

    # Step-by-step mode sends one command at a time and waits for
    # the response. The purpose is to test if the device processes
    # requests as supposed.
    print >>sys.stderr, "Testing in step-by-step mode ...",
    runTest(dev, 0, 1)

    # Fast-write mode sends and receives as fast as possible. The purpose
    # is to test whether the device correctly throttles the host when
    # its receive buffer fills up.
    print >>sys.stderr, "Testing in fast-write mode ...",
    runTest(dev, 1, 1)

    # Throttled-read mode sends commands as fast as possible, and reads
    # responses slower than the device generates them. The purpose is to
    # test whether the device works correctly when it is throttled by the
    # host and its transmission buffer fills up.
    print >>sys.stderr, "Testing in throttled-read mode ...",
    runTest(dev, 1, 0)

    print >>sys.stderr, "Switching back to text mode ..."
    dev.write('\x00\x00')

    print >>sys.stderr, "All done."
    dev.close()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print >>sys.stderr, __doc__
        sys.exit(1)
    testdev(sys.argv[1])

