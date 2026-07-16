import os
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker, declarative_base
from dotenv import load_dotenv

# Load environment variables dari file .env
load_dotenv()

DATABASE_URL = os.getenv("DATABASE_URL")

# Debug: print untuk cek apakah URL terbaca
print(f"DATABASE_URL: {DATABASE_URL}")

if not DATABASE_URL:
    raise ValueError("DATABASE_URL tidak ditemukan! Pastikan file .env ada dan berisi DATABASE_URL")

engine = create_engine(DATABASE_URL)

SessionLocal = sessionmaker(
    autocommit=False,
    autoflush=False,
    bind=engine
)

Base = declarative_base()

def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

if __name__ == "__main__":
    try:
        conn = engine.connect()
        print("Database terhubung!")
        conn.close()
    except Exception as e:
        print("Error:", e)