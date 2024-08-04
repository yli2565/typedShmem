import io
from logging import Logger,getLogger
from mmap import mmap
from typing import Any, List, Tuple, Union
import tqdm
from abc import ABC, abstractmethod
from PrimitiveTypes import *

StreamAlike = Union[mmap, Tuple[memoryview, int], io.BytesIO, bytes]

class StreamAlikeWrapper(ABC):
    "Wrap any StreamAlike object to make a uniform interface with StreamUtil"

    def __init__(self, stream: StreamAlike, showProgress=False, desc="Streaming"):
        """Any object that supports read(), seek(), tell(), close(), but it might trigger warnings from StreamAlike type check"""
        self._stream: Union[mmap, memoryview, io.BytesIO]
        self._pbar: tqdm.tqdm

        self._index: int

        # Init logger
        self.logger: Logger = getLogger(__name__)

        if isinstance(stream, bytes):
            self._stream = io.BytesIO(stream)
            self._index = 0
        elif isinstance(stream, tuple):
            if len(stream) == 2 and isinstance(stream[0], memoryview):
                # (memoryview, index)
                self._stream = stream[0]
                self._index = stream[1]
            else:
                errMsg = f"Tuple {stream} not supported"
                self.logger.error(errMsg)
                raise TypeError(errMsg)
        elif isinstance(stream, io.BytesIO) or isinstance(
            stream, mmap
        ):  # it's a mmap or io.BytesIO
            self._stream = stream  # type: ignore
            self._index = stream.tell()
        else:
            if (
                hasattr(stream, "read")
                and hasattr(stream, "tell")
                and hasattr(stream, "seek")
            ):
                self._stream = stream
                self._index = stream.tell()
            else:
                errMsg = f"Stream {type(stream)} not supported"
                self.logger.error(errMsg)
                raise TypeError(errMsg)

        # Init pbar
        self._pbar = tqdm.tqdm(
            total=self.size(),
            unit_scale=True,
            unit_divisor=1024,
            position=0,
            desc=desc,
            disable=not showProgress,
        )

    # Basic Stream Methods
    @property
    def stream(self) -> Union[mmap, memoryview, io.BytesIO]:
        if self._stream is not None:
            return self._stream
        else:
            errMsg = f"Stream not set"
            self.logger.error(errMsg)
            raise ValueError(errMsg)

    def updatePbar(self):
        currentPos = self._pbar.n
        if currentPos < self._index:
            self._pbar.update(currentPos - self._index)
        else:
            self.logger.warning(f"pbar is going backward, pbar pos: {currentPos}, stream index: {self._index}")

    def read(self, numBytes):
        remainingSize = self.remainingSize()
        if remainingSize < numBytes:
            self.logger.error(
                f"Not enough data to read in {type(self.stream)}. Remaining: {remainingSize}, Requested: {numBytes}\n\t
                {self.probe(remainingSize)}"
            )
            raise EOFError("Not enough data to read")

        if isinstance(self.stream, memoryview):
            result = self.stream[self._index : self._index + numBytes].tobytes()
        else:
            result = self.stream.read(numBytes)

        self._index += numBytes

        # Update pbar
        self.updatePbar()
        return result

    def tell(self) -> int:
        """Current position of StreamUtil's stream"""
        if isinstance(self.stream, memoryview):
            return self._index
        return self.stream.tell()

    def seek(self, offset, whence=0) -> None:
        if whence == io.SEEK_CUR:
            self._index += int(offset)
        elif whence == io.SEEK_SET:
            self._index = int(offset)
        elif whence == io.SEEK_END:
            self._index = self.size() - int(offset)

        # Update stream only if stream keeps its own head
        if not isinstance(self.stream, memoryview):
            self.stream.seek(self._index, io.SEEK_SET)

        # Update pbar
        self.updatePbar()

    def size(self) -> int:
        """Total size of StreamUtil's stream"""
        if isinstance(self.stream, io.BytesIO):
            return len(self.stream.getvalue())
        elif isinstance(self.stream, mmap):
            return self.stream.size()
        elif isinstance(self.stream, memoryview):
            return self.stream.nbytes
        else:
            self.logger.warning(
                f"Unknown stream type {type(self.stream)}, using tell() and seek() to get size"
            )
            pos = self.tell()
            self.stream.seek(0, io.SEEK_END)  # Don't update the pbar
            size = self.tell()
            self.stream.seek(pos)  # Don't update the pbar
            return size

    def probe(self, numBytes) -> bytes:
        """Read some bytes without moving the cursor"""
        if isinstance(self.stream, memoryview):
            result = self.stream[self.tell() : self.tell() + numBytes].tobytes()
        else:
            # Directly use read and seek on stream to prevent pbar update
            result = self.stream.read(numBytes)
            self.stream.seek(-numBytes, io.SEEK_CUR)
        return result

    def close(self):
        if isinstance(self.stream, memoryview):
            self.stream.release()
        else:
            self.stream.close()

    def getAllBytes(self) -> bytes:
        """Get all bytes of StreamUtil's stream"""
        if isinstance(self.stream, io.BytesIO):
            return self.stream.getvalue()
        elif isinstance(self.stream, mmap):
            return self.stream[:]
        elif isinstance(self.stream, memoryview):
            return self.stream.tobytes()
        else:
            try:
                self.logger.warning(
                    f"Unknown stream type {type(self.stream)}, using tell(), read() and seek() to get all bytes"
                )
                currentPos = self.tell()
                self.seek(0)
                result = self.read(self.size())
                self.seek(currentPos)
                return result
            except:
                errMsg=f"Stream {type(self.stream)} not supported"
                self.logger.error(errMsg)
                raise TypeError(errMsg)

    def remainingSize(self) -> int:
        return self.size() - self.tell()

    def atEnd(self):
        if self.tell() == self.size():
            return True
        elif self.tell() > self.size():
            errMsg=f"Stream {type(self.stream)}'s cursor is out of range."
            self.logger.error(errMsg+f" Size: {self.size()}, Cursor: {self.tell()}")
            raise IndexError(errMsg)
        else:
            return False
