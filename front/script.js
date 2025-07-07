let aliranHidup = true;
let modalCallback = null;

function showAliranModal() {
  const modal = document.getElementById("aliranModal");
  const body = document.getElementById("modalBody");
  body.textContent = aliranHidup ? "Matikan aliran listrik?" : "Nyalakan aliran listrik?";
  modal.style.display = "flex";
  // Set callback
  modalCallback = function (yes) {
    if (yes) toggleAliran();
    modal.style.display = "none";
  };
}

function confirmAliran(yes) {
  if (modalCallback) modalCallback(yes);
}

function toggleAliran() {
  aliranHidup = !aliranHidup;
  const btn = document.getElementById("aliranBtn");
  const status = document.getElementById("aliranStatus");
  if (aliranHidup) {
    btn.textContent = "Matikan Aliran?";
    btn.classList.remove("nyala");
    status.textContent = "Aliran Hidup";
  } else {
    btn.textContent = "Nyalakan Aliran?";
    btn.classList.add("nyala");
    status.textContent = "Aliran Mati";
  }
}
