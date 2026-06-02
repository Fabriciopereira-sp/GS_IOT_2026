# 🛰️ AgroSat Station — IoT Global Solution FIAP 2026/1

Estação de monitoramento de solo inspirada no uso de dados satelitais para o agronegócio.
O ESP32 coleta dados ambientais e os disponibiliza via API REST com dashboard web.

## 🔗 Links

| | |
|---|---|
| 🔧 Simulação Wokwi | [Abrir projeto](https://wokwi.com/projects/465560212441654273) |
| 🎥 Vídeo de apresentação | _(adicionar link do YouTube após gravar)_ |

## 👥 Integrantes — Turma 2TDSPW

| Nome | RM |
|------|----|
| Fabrício Henrique Pereira | 563237 |
| Leonardo José Pereira | 563065 |
| Miguel Henrique Oliveira Dias | 565492 |
| Pedro Henrique de Oliveira | 562312 |
| Henrique Sinkevicius Maran | 562977 |

## 📡 Contexto

A economia espacial permite que satélites monitorem remotamente condições climáticas e de solo.
Este protótipo simula o nó terrestre desse sistema: coleta temperatura, umidade do ar e umidade
do solo, emite alertas sonoros e visuais e expõe os dados via Wi-Fi.

## 🔧 Componentes

| Componente | Função | Pino |
|---|---|---|
| DHT22 | Entrada 1 — temperatura e umidade do ar | GPIO 4 |
| Sensor de umidade do solo | Entrada 2 — umidade do solo | GPIO 34 |
| LED Verde | Saída 1 — status normal | GPIO 26 |
| LED Vermelho | Saída 2 — alerta piscante | GPIO 27 |
| Buzzer | Saída 3 — alerta sonoro | GPIO 25 |
| LCD I2C 16x2 | Interface local | SDA 21 / SCL 22 |

## 🌐 Endpoints da API

| Método | Rota | Descrição |
|--------|------|-----------|
| GET | `/` | Dashboard HTML com gráficos históricos |
| GET | `/status` | JSON completo com histórico |
| GET | `/temp` | JSON só com temperatura |
| GET | `/humidity` | JSON só com umidade do ar |
| GET | `/soil` | JSON só com umidade do solo |

### Exemplo — `/status`
```json
{
  "temperatura_c": 28.50,
  "umidade_ar_pct": 62.00,
  "solo_umidade_pct": 45,
  "solo_raw": 2200,
  "alerta": false,
  "mensagem": "OK",
  "historico_temp": [27.1, 27.5, 28.0, 28.5],
  "historico_hum": [63.0, 62.5, 62.0, 62.0],
  "historico_solo": [46, 45, 45, 45],
  "uptime_ms": 12400
}
```

## ⚠️ Lógica de Alertas

| Condição | LED Verde | LED Vermelho | Buzzer | LCD |
|---|---|---|---|---|
| Tudo normal | Aceso | Apagado | Silencioso | OK |
| Temperatura > 35°C | Apagado | Piscando | Bipa a cada 2s | CALOR EXCESSIVO |
| Umidade ar < 30% | Apagado | Piscando | Bipa a cada 2s | SECA NO AR |
| Solo < 20% | Apagado | Piscando | Bipa a cada 2s | SOLO MUITO SECO |

## 🚀 Como executar no Wokwi

1. Acesse o projeto direto: [wokwi.com/projects/465560212441654273](https://wokwi.com/projects/465560212441654273)
2. Clique em **▶ Start Simulation**
3. Copie o IP exibido no Serial Monitor
4. Clique em **Open in browser** para ver o dashboard

## 🎓 FIAP — Análise e Desenvolvimento de Sistemas

**Disciplina:** Disruptive Architectures: IoT, IoB & Generative IA  
**Global Solution 2026/1 — Tema: A Economia Espacial**
