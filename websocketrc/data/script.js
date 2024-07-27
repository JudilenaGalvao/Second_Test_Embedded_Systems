//https://javascript.info/websocket
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
// Init web socket when the page loads
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
}

function getReadings(){
    websocket.send("getReadings");
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

// When websocket is established, call the getReadings() function
function onOpen(event) {
    console.log('Connection opened');
    getReadings();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

var tensao = 0
var v = 0

var time = 0
var t = 0

// Function that receives the message from the ESP32 with the readings
function onMessage(event) {
    console.log(event.data);
    
    // voltagem para ser usada no grafico
    v = event.data
    v = v.slice(v.indexOf("PV"))
    v = v.slice(5,9)    
    v = parseFloat(v)
    tensao = v

    // tempo para ser usado no grafico
    t = event.data
    t = t.slice(t.indexOf(":"),t.indexOf(","))
    t = t.slice(2,-1)
    t = parseInt(t)
    time = t

    var myObj = JSON.parse(event.data);
    var keys = Object.keys(myObj);

    for (var i = 0; i < keys.length; i++){
        var key = keys[i];
        document.getElementById(key).innerHTML = myObj[key];
    }
}

// grafico
var chart = new CanvasJS.Chart("chartContainer", {
    title: {
        text: "Monitoramento de Voltagem ao Longo do Tempo"
    },
    axisX: {
        title: "Tempo (segundos)"
    },
    axisY: {
        title: "Voltagem (V)"
    },
    data: [{
        type: "line",
        dataPoints: []
    }]
});

var tempo = 0; // Variável para controlar o tempo
var intervalo = setInterval(function () {
    
    var voltagem = tensao

    // Adicionar ponto de dados ao gráfico
    chart.options.data[0].dataPoints.push({ x: tempo, y: voltagem });

    // Atualizar o tempo
    tempo = time;

    // Limitar o número de pontos de dados mostrados (opcional)
    if (chart.options.data[0].dataPoints.length > 20) {
        chart.options.data[0].dataPoints.shift(); // Remove o primeiro elemento do array
    }

    // Renderizar o gráfico
    chart.render();
}, 1000); // Atualizar a cada segundo (1000 milissegundos)
// ===

function selecionarGrafico(){
    let x = document.getElementById('chartContainer');
    if (x.style.visibility === 'hidden') {
        x.style.visibility = 'visible';
    } else {
        x.style.visibility = 'hidden';
    }

    let y = document.getElementById('card');
    if (y.style.visibility === 'hidden') {
        y.style.visibility = 'visible';
    } else {
        y.style.visibility = 'hidden';
    }
}