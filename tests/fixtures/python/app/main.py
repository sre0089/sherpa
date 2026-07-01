from .service import Service
from app.utils import helper
import external_package


async def run(value: int) -> int:
    service = Service()
    return service.process(helper(value))
