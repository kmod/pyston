# Adapted from CPython's Dockerfile:

FROM buildpack-deps:jessie

# http://bugs.python.org/issue19846
# > At the moment, setting "LANG=C" on a Linux system *fundamentally breaks Python 3*, and that's not OK.
ENV LANG C.UTF-8

ENV PYSTON_TAG v0.4
ENV PYSTON_VERSION 0.4.0

RUN set -x \
    && curl -SL "https://github.com/dropbox/pyston/releases/download/${PYSTON_TAG}/pyston-${PYSTON_VERSION}-linux64.tar.gz" | tar -xzC / \
    && ln -s /pyston-${PYSTON_VERSION}-linux64/pyston /usr/local/bin

RUN pyston /pyston-${PYSTON_VERSION}-linux64/virtualenv/virtualenv.py -p pyston /test_env

ENV PATH /test_env/bin/:$PATH

RUN pip install https://github.com/dropbox/pyston/releases/download/v0.4/Cython-0.22-pyston.tar.gz

CMD ["python"];
