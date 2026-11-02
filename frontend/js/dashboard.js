async function carregarDashboard() {
  const statusEl = document.getElementById("status-api");
  const seletor = document.getElementById("filtro-estacao");
  const kpis = {
    temperatura: document.getElementById("kpi-temperatura"),
    umidade: document.getElementById("kpi-umidade"),
    pressao: document.getElementById("kpi-pressao"),
    ar: document.getElementById("kpi-ar"),
    luz: document.getElementById("kpi-luz"),
    mediaTemperatura: document.getElementById("kpi-media-temperatura"),
    mediaUmidade: document.getElementById("kpi-media-umidade"),
    chuva: document.getElementById("kpi-chuva"),
    alertas: document.getElementById("kpi-alertas"),
  };

  try {
    const [saude, resumo, estacoes, ultimas, alertas] = await Promise.all([
      ApiEstacao.saude(),
      ApiEstacao.resumo(),
      ApiEstacao.estacoes(),
      ApiEstacao.ultimas(),
      ApiEstacao.alertas(true),
    ]);

    const maisRecente = ultimas.length > 0 ? ultimas[0] : null;

    if (!maisRecente) {
      statusEl.innerHTML = '<span class="inline-flex items-center gap-2"><span class="w-2.5 h-2.5 rounded-full bg-amber-400 animate-pulse"></span> API online · Aguardando primeira conexão do ESP32...</span>';
    } else {
      // Tempo decorrido desde a última leitura recebida
      const agora = new Date().getTime();
      const dataUltimaLeitura = new Date(maisRecente.coletada_em).getTime();
      const diferencaSegundos = Math.max(0, Math.floor((agora - dataUltimaLeitura) / 1000));

      // Se a última leitura foi há até 20 segundos, o ESP32 está ativamente conectado (com envio de 5s)
      if (diferencaSegundos <= 20) {
        statusEl.innerHTML = `<span class="inline-flex items-center gap-2"><span class="w-2.5 h-2.5 rounded-full bg-emerald-400 animate-pulse"></span> API online · ESP32 conectado · Última leitura: ${UtilEstacao.formatarData(maisRecente.coletada_em)} (${diferencaSegundos}s atrás)</span>`;
      } else {
        let textoTempo = `${diferencaSegundos}s atrás`;
        if (diferencaSegundos >= 60) {
          const minutos = Math.floor(diferencaSegundos / 60);
          textoTempo = minutos === 1 ? '1 min atrás' : `${minutos} min atrás`;
        }
        statusEl.innerHTML = `<span class="inline-flex items-center gap-2"><span class="w-2.5 h-2.5 rounded-full bg-rose-400"></span> API online · ESP32 desconectado (última leitura ${textoTempo}) · Aguardando reconexão...</span>`;
      }
    }

    // 1. CARDS DE MEDIÇÃO EM TEMPO REAL (Valores instantâneos do sensor)
    kpis.temperatura.textContent = maisRecente
      ? UtilEstacao.formatarNumero(maisRecente.temperatura, 1, " °C")
      : "--";

    kpis.umidade.textContent = maisRecente
      ? UtilEstacao.formatarNumero(maisRecente.umidade, 1, " %")
      : "--";

    kpis.pressao.textContent = maisRecente
      ? UtilEstacao.formatarNumero(maisRecente.pressao, 1, " hPa")
      : "--";

    kpis.ar.textContent = maisRecente
      ? UtilEstacao.formatarNumero(maisRecente.qualidade_ar, 0, " ppm")
      : "--";

    if (kpis.luz) {
      kpis.luz.textContent = maisRecente
        ? UtilEstacao.formatarNumero(maisRecente.luminosidade, 0, " %")
        : "--";
    }

    // 2. CARDS DE MÉDIAS E ACUMULADOS DE 24H
    if (kpis.mediaTemperatura) {
      kpis.mediaTemperatura.textContent = UtilEstacao.formatarNumero(resumo.media_temperatura, 1, " °C");
    }
    if (kpis.mediaUmidade) {
      kpis.mediaUmidade.textContent = UtilEstacao.formatarNumero(resumo.media_umidade, 1, " %");
    }
    kpis.chuva.textContent = UtilEstacao.formatarNumero(resumo.acumulado_chuva_24h, 1, " mm");
    kpis.alertas.textContent = String(resumo.alertas_ativos);

    const valorAtual = seletor.value;
    seletor.innerHTML = estacoes.length > 1 ? '<option value="">Todas as estações</option>' : '';
    estacoes.forEach((estacao) => {
      const option = document.createElement("option");
      option.value = estacao.id;
      option.textContent = estacao.nome;
      seletor.appendChild(option);
    });
    if (valorAtual) {
      seletor.value = valorAtual;
    }

    const estacaoId = seletor.value;
    const serie = await ApiEstacao.leituras(estacaoId, 48, true);
    GraficosEstacao.renderizarClima("grafico-clima", serie);
    GraficosEstacao.renderizarArChuva("grafico-ar", serie);

    const tbody = document.getElementById("tabela-ultimas");
    tbody.innerHTML = "";
    if (ultimas.length === 0) {
      tbody.innerHTML = `
        <tr>
          <td colspan="7" class="px-4 py-8 text-center text-slate-400">
            <p class="font-medium text-slate-300">Nenhuma leitura recebida ainda.</p>
            <p class="text-xs text-cyan-300 mt-1">Conecte a placa ESP32 ao Wi-Fi para iniciar o monitoramento em tempo real.</p>
          </td>
        </tr>
      `;
    } else {
      ultimas.forEach((leitura) => {
        const estacao = estacoes.find((item) => item.id === leitura.estacao_id);
        const tr = document.createElement("tr");
        tr.innerHTML = `
          <td class="px-3 py-2 font-medium">${estacao ? estacao.nome : "ESP32 (" + leitura.estacao_id + ")"}</td>
          <td class="px-3 py-2 font-semibold text-cyan-300">${UtilEstacao.formatarNumero(leitura.temperatura, 1, " °C")}</td>
          <td class="px-3 py-2">${UtilEstacao.formatarNumero(leitura.umidade, 0, "%")}</td>
          <td class="px-3 py-2">${UtilEstacao.formatarNumero(leitura.pressao, 1)}</td>
          <td class="px-3 py-2">
            <span class="px-2 py-1 rounded-full text-xs ${UtilEstacao.classeAr(leitura.classificacao_ar)}">
              ${UtilEstacao.rotuloAr(leitura.classificacao_ar)}
            </span>
          </td>
          <td class="px-3 py-2">${UtilEstacao.formatarNumero(leitura.pluviometria, 1, " mm")}</td>
          <td class="px-3 py-2">${UtilEstacao.formatarData(leitura.coletada_em)}</td>
        `;
        tbody.appendChild(tr);
      });
    }

    const listaAlertas = document.getElementById("lista-alertas");
    listaAlertas.innerHTML = "";
    if (alertas.length === 0) {
      listaAlertas.innerHTML = '<div class="glass rounded-xl p-4 text-slate-300 text-sm">Nenhum alerta climático ativo neste momento.</div>';
    } else {
      alertas.slice(0, 6).forEach((alerta) => {
        const card = document.createElement("article");
        card.className = "glass rounded-xl p-4";
        card.innerHTML = `
          <div class="flex items-center justify-between gap-3 mb-2">
            <h3 class="font-semibold">${alerta.estacao_nome || "Estação ESP32"}</h3>
            <span class="${UtilEstacao.classeSeveridade(alerta.severidade)}">${UtilEstacao.rotuloSeveridade(alerta.severidade)}</span>
          </div>
          <p class="text-sm text-slate-300">${alerta.mensagem}</p>
          <p class="text-xs text-slate-400 mt-2">${UtilEstacao.formatarData(alerta.criado_em)}</p>
        `;
        listaAlertas.appendChild(card);
      });
    }
  } catch (erro) {
    statusEl.textContent = `Falha ao conectar na API: ${erro.message}`;
  }
}

document.addEventListener("DOMContentLoaded", () => {
  const seletor = document.getElementById("filtro-estacao");
  seletor.addEventListener("change", carregarDashboard);
  carregarDashboard();
  // Atualiza a cada 4 segundos para resposta rápida do ESP32
  window.setInterval(carregarDashboard, 4000);
});
