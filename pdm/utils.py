def parse_range(range, check = True):
    """ Turn a:b into a float tuple. a or b can be missed, None will be returned.

    >>> parse_range('0.75:1', check=False)
    (0.75, 1.0)
    >>> parse_range(':-4')
    (None, -4.0)
    >>> parse_range('12:')
    (12.0, None)
    """
    a, b = range.split(':')
    a = float(a) if a else None
    b = float(b) if b else None

    if check:
        if a is None and b is None:
            raise ValueError("Both ends of the range '%s' can't be empty" % range)
        if a is not None and b is not None and a > b:
            raise ValueError("Range start must be <= end ('%s')" % range)
    return (a, b)


