from fastapi import FastAPI, Depends, HTTPException, Request
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from sqlalchemy.orm import Session
from pydantic import BaseModel, Field
from typing import List
import uvicorn
from datetime import datetime

from database import engine, get_db, Base
import models

# Membuat semua tabel di database
Base.metadata.create_all(bind=engine)

app = FastAPI(title="IoT Data Receiver", version="1.0.0")

# Setup templates
templates = Jinja2Templates(directory="templates")

# Pydantic model untuk validasi data yang diterima
class IoTDataCreate(BaseModel):
    voltage: float = Field(..., description="Tegangan dalam Volt")
    current: float = Field(..., description="Arus dalam Ampere")
    power: float = Field(..., description="Daya dalam Watt")
    energy: float = Field(..., description="Energi dalam kWh")
    pf: float = Field(..., description="Power Factor")
    ssr_state: bool = Field(..., description="Status SSR")

    class Config:
        json_schema_extra = {
            "example": {
                "voltage": 220.5,
                "current": 1.5,
                "power": 330.75,
                "energy": 0.1250,
                "pf": 0.85,
                "ssr_state": True
            }
        }

# Pydantic model untuk response
class IoTDataResponse(BaseModel):
    id: int
    voltage: float
    current: float
    power: float
    energy: float
    pf: float
    ssr_state: bool
    created_at: datetime

    class Config:
        from_attributes = True

# Endpoint untuk menerima data IoT (POST)
@app.post("/api/data", response_model=IoTDataResponse)
async def receive_iot_data(data: IoTDataCreate, db: Session = Depends(get_db)):
    """
    Endpoint untuk menerima data dari perangkat IoT
    """
    try:
        db_data = models.IoTData(
            voltage=data.voltage,
            current=data.current,
            power=data.power,
            energy=data.energy,
            pf=data.pf,
            ssr_state=data.ssr_state
        )
        
        db.add(db_data)
        db.commit()
        db.refresh(db_data)
        
        return db_data
    
    except Exception as e:
        db.rollback()
        raise HTTPException(status_code=500, detail=f"Error menyimpan data: {str(e)}")

# Endpoint untuk mendapatkan semua data
@app.get("/api/data", response_model=List[IoTDataResponse])
async def get_all_data(skip: int = 0, limit: int = 100, db: Session = Depends(get_db)):
    """
    Endpoint untuk mengambil semua data IoT
    """
    data = db.query(models.IoTData)\
        .order_by(models.IoTData.created_at.desc())\
        .offset(skip)\
        .limit(limit)\
        .all()
    
    return data

# Endpoint untuk mendapatkan data terbaru
@app.get("/api/data/latest", response_model=IoTDataResponse)
async def get_latest_data(db: Session = Depends(get_db)):
    """
    Endpoint untuk mengambil data terbaru
    """
    data = db.query(models.IoTData)\
        .order_by(models.IoTData.created_at.desc())\
        .first()
    
    if not data:
        raise HTTPException(status_code=404, detail="Tidak ada data")
    
    return data

# Endpoint untuk halaman monitoring
@app.get("/", response_class=HTMLResponse)
async def monitoring_dashboard(request: Request):
    """
    Halaman dashboard monitoring IoT
    """
    return templates.TemplateResponse("index.html", {"request": request})

# Health check endpoint
@app.get("/health")
async def health_check():
    return {"status": "healthy", "message": "Server berjalan normal"}

# Endpoint khusus untuk SSR Status (sesuai request dari IoT)
@app.get("/api/data/ssr-status")
async def get_ssr_status(db: Session = Depends(get_db)):
    """
    Endpoint untuk mendapatkan status SSR terbaru
    """
    try:
        data = db.query(models.IoTData)\
            .order_by(models.IoTData.created_at.desc())\
            .first()
        
        if not data:
            return {"ssr_state": False, "message": "No data available"}
        
        return {
            "ssr_state": data.ssr_state,
            "voltage": data.voltage,
            "current": data.current,
            "power": data.power,
            "energy": data.energy,
            "pf": data.pf,
            "created_at": data.created_at.isoformat()
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Error: {str(e)}")

# Endpoint untuk update SSR state (jika diperlukan)
@app.post("/api/data/ssr-status")
async def update_ssr_status(ssr_state: bool = True, db: Session = Depends(get_db)):
    """
    Endpoint untuk update status SSR
    """
    try:
        # Ambil data terbaru
        data = db.query(models.IoTData)\
            .order_by(models.IoTData.created_at.desc())\
            .first()
        
        if data:
            data.ssr_state = ssr_state
            db.commit()
            return {"message": "SSR status updated", "ssr_state": ssr_state}
        else:
            return {"message": "No data to update", "ssr_state": None}
    except Exception as e:
        db.rollback()
        raise HTTPException(status_code=500, detail=f"Error: {str(e)}")

if __name__ == "__main__":
    # Jalankan server di semua interface jaringan (0.0.0.0) agar bisa diakses dari jaringan lokal
    uvicorn.run(
        "main:app",
        host="0.0.0.0",
        port=8000,
        reload=True,
        log_level="info"
    )