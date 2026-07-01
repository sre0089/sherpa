from .utils import helper


class Service:
    def process(self, value: int) -> int:
        return helper(value)

    def twice(self, value: int) -> int:
        return self.process(value) + self.process(value)
