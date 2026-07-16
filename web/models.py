from sqlalchemy import Column, Integer, Float, Boolean, DateTime
from sqlalchemy.sql import func
from database import Base


class IoTData(Base):
    __tablename__ = "iot_data"

    id = Column(Integer, primary_key=True, index=True, autoincrement=True)
    voltage = Column(Float, nullable=False)
    current = Column(Float, nullable=False)
    power = Column(Float, nullable=False)
    energy = Column(Float, nullable=False)
    pf = Column(Float, nullable=False)
    ssr_state = Column(Boolean, nullable=False)
    created_at = Column(DateTime(timezone=True), server_default=func.now())

    def __repr__(self):
        return f"<IoTData(voltage={self.voltage}, current={self.current}, power={self.power})>"