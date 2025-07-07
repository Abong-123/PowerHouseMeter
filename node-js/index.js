const express = require("express");
const cors = require("cors");
const bodyParser = require("body-parser");
const app = express();


// Configuration (from cPanel Environment Variables)
const CONFIG = {
  API_KEY: process.env.API_KEY || "Kunci_Surga", // Fallback for local development
  PORT: process.env.PORT || 3000,
  CORS_ORIGIN: process.env.CORS_ORIGIN || "https://powerhousemeter.tk2b.my.id",
};

// System State (In-Memory Storage)
let systemState = {
  ssrStatus: true,
  lastData: null,
  lastUpdate: null,
  overCurrentTriggered: false,
};

// Middleware
app.use(cors({ origin: CONFIG.CORS_ORIGIN }));
app.use(bodyParser.json());

// API Key Authentication
const authenticateKey = (req, res, next) => {
  const clientKey = req.headers["x-api-key"];

  if (!clientKey) {
    console.warn(`[${new Date().toISOString()}] Unauthorized: Missing API Key from ${req.ip}`);
    return res.status(401).json({ error: "API Key required" });
  }

  if (clientKey !== CONFIG.API_KEY) {
    console.warn(`[${new Date().toISOString()}] Unauthorized: Invalid Key from ${req.ip}`);
    return res.status(403).json({ error: "Invalid API Key" });
  }

  next();
};

// API Endpoints
// ======================================

// Health Check
app.get("/", (req, res) => {
  res.json({
    status: "active",
    app: CONFIG.APP_NAME,
    version: "1.1.0",
  });
});

// IoT Data Endpoint
app.post("/api/iot/data", authenticateKey, (req, res) => {
  const { voltage, current, power, energy, pf } = req.body;

  // Data Validation
  if (!voltage || current === undefined || !power || !energy) {
    return res.status(400).json({ error: "Missing required sensor data" });
  }

  // Over-Current Protection
  if (current > OVER_CURRENT_LIMIT) {
    systemState.overCurrentTriggered = true;
    systemState.ssrStatus = false;
  }

  // Update System State
  systemState = {
    ...systemState,
    lastData: {
      voltage,
      current,
      power,
      energy,
      pf: pf || null,
      timestamp: new Date().toISOString(),
    },
    lastUpdate: new Date().toISOString(),
  };

  console.log(`[${systemState.lastUpdate}] Data Received`, {
    Voltage: `${voltage}V`,
    Current: `${current}A`,
    Power: `${power}W`,
    SSR: systemState.ssrStatus ? "ON" : "OFF",
  });

  res.json({
    success: true,
    ssr_status: systemState.ssrStatus,
    received_data: systemState.lastData,
  });
});

// SSR Control Endpoints
app.get("/api/iot/status", authenticateKey, (req, res) => {
  res.json({
    ssr_status: systemState.ssrStatus,
    last_update: systemState.lastUpdate,
    system_health: {
      over_current: systemState.overCurrentTriggered,
      last_voltage: systemState.lastData?.voltage || null,
    },
  });
});

app.post("/api/iot/control", authenticateKey, (req, res) => {
  const { action } = req.body;

  if (action === "reset") {
    systemState.overCurrentTriggered = false;
    systemState.ssrStatus = true;
    console.log(`[${new Date().toISOString()}] System Reset`);
  } else if (action === "on") {
    systemState.ssrStatus = true;
    console.log(`[${new Date().toISOString()}] SSR Manually Activated`);
  } else if (action === "off") {
    systemState.ssrStatus = false;
    console.log(`[${new Date().toISOString()}] SSR Manually Deactivated`);
  } else {
    return res.status(400).json({ error: "Invalid action. Use 'on', 'off', or 'reset'" });
  }

  res.json({
    success: true,
    ssr_status: systemState.ssrStatus,
    over_current_lock: systemState.overCurrentTriggered,
  });
});


// Error Handling
app.use((err, req, res, next) => {
  console.error(`[${new Date().toISOString()}] Error:`, err.stack);
  res.status(500).json({
    error: "Internal Server Error",
    message: err.message,
  });
});

// Server Startup
app.listen(CONFIG.PORT, () => {
  console.log(`⚡ Powerhouse Backend v1.1.0`);
  console.log(`🔗 Base URL: http://localhost:${CONFIG.PORT}`);
  console.log(`🔐 API Key: ${CONFIG.API_KEY.substring(0, 3)}...${CONFIG.API_KEY.substring(-3)}`);
  console.log(`🌐 Allowed Origin: ${CONFIG.CORS_ORIGIN}`);
});
