import logging
import time
import threading
from typing import Dict, Tuple, Optional

class RateLimitedFormatter(logging.Formatter):
    """
    Standard formatter for PhysAnim logs.
    """
    def __init__(self, fmt=None, datefmt=None, style='%'):
        if fmt is None:
            fmt = '[%(asctime)s][%(levelname)s] %(message)s'
        super().__init__(fmt, datefmt, style)

class PhysAnimLogger:
    """
    PhysAnimLogger
    A rate-limited logging wrapper designed to prevent log spam in Python scripts.
    Matches the behavior of FPhysAnimLogger in C++.
    """
    _instance = None
    _lock = threading.Lock()

    def __init__(self):
        if PhysAnimLogger._instance is not None:
            raise Exception("This class is a singleton!")
        
        self.logger = logging.getLogger("PhysAnim")
        self.logger.setLevel(logging.INFO)
        
        # Default handler to console
        handler = logging.StreamHandler()
        handler.setFormatter(RateLimitedFormatter())
        self.logger.addHandler(handler)
        
        self.log_history: Dict[Tuple[str, int], Dict] = {}
        self.history_lock = threading.Lock()

    @classmethod
    def get_instance(cls) -> 'PhysAnimLogger':
        if cls._instance is None:
            with cls._lock:
                if cls._instance is None:
                    cls._instance = PhysAnimLogger()
        return cls._instance

    def log_rate_limited(self, level: int, file: str, line: int, time_limit: float, message: str, *args):
        key = (file, line)
        current_time = time.time()
        
        with self.history_lock:
            if key not in self.log_history:
                self.log_history[key] = {'last_time': 0.0, 'suppressed_count': 0}
            
            record = self.log_history[key]
            
            if current_time - record['last_time'] >= time_limit:
                suppressed = record['suppressed_count']
                record['last_time'] = current_time
                record['suppressed_count'] = 0
                
                final_message = message % args if args else message
                if suppressed > 0:
                    final_message = f"[Suppressed {suppressed} times] {final_message}"
                
                self.logger.log(level, final_message)
            else:
                record['suppressed_count'] += 1

def info(message: str, time_limit: float = 0.0, *args):
    import inspect
    frame = inspect.currentframe().f_back
    PhysAnimLogger.get_instance().log_rate_limited(logging.INFO, frame.f_code.co_filename, frame.f_lineno, time_limit, message, *args)

def warning(message: str, time_limit: float = 0.0, *args):
    import inspect
    frame = inspect.currentframe().f_back
    PhysAnimLogger.get_instance().log_rate_limited(logging.WARNING, frame.f_code.co_filename, frame.f_lineno, time_limit, message, *args)

def error(message: str, time_limit: float = 0.0, *args):
    import inspect
    frame = inspect.currentframe().f_back
    # Errors are never suppressed as per PRD mitigation
    PhysAnimLogger.get_instance().log_rate_limited(logging.ERROR, frame.f_code.co_filename, frame.f_lineno, 0.0, message, *args)

def debug(message: str, time_limit: float = 0.0, *args):
    import inspect
    frame = inspect.currentframe().f_back
    PhysAnimLogger.get_instance().log_rate_limited(logging.DEBUG, frame.f_code.co_filename, frame.f_lineno, time_limit, message, *args)
