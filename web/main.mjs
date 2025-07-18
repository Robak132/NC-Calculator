/** @typedef Component
 * @property {string} title
 * @property {string} name
 * @property {string} className
 * @property {number} cooling_rate
 * @property {number} active_cooling_rate
 * @property {number} id
 */

/** @type {Component[]} */
import COMPONENTS from "../data/components.json" with {type: "json"};
import "https://cdn.jsdelivr.net/npm/chart.js@2.9.3/dist/Chart.min.js"
import "https://code.jquery.com/jquery-3.5.0.slim.min.js"
import "./FissionOpt.js";
import {Int16, Int32, write} from "https://cdn.jsdelivr.net/npm/nbtify@2.2.0/+esm";

function createBlockTypeTable() {
  const table = $('<table></table>');
  [
    { id: "blockType", label: 'Block Type', getValue: bt => `<th title="${bt.title}" class="${bt.className}">${bt.name}</th>` },
    { id: "rate", label: 'Cooling Rate (H/t)', getValue: bt => bt.cooling_rate ? `<td><input value="${bt.cooling_rate}"></td>` : `<td>&mdash;</td>` },
    { id: "limit", label: 'Max Allowed', getValue: () => '<td><input type="text"></td>' },
    { id: "activeRate", label: 'Active Cooling Rate (H/t)', getValue: bt => bt.active_cooling_rate ? `<td><input type="text" value="${bt.active_cooling_rate}"></td>` : `<td>&mdash;</td>` },
    { id: "activeLimit", label: 'Max Active Allowed', getValue: bt => bt.active_cooling_rate ? '<td><input type="text" value="0"></td>' : `<td>&mdash;</td>` }
  ].forEach(rowDef => {
    const row = $(`<tr id="${rowDef.id}"></tr>`);
    row.append(`<th>${rowDef.label}</th>`);
    COMPONENTS.forEach(bt => row.append(rowDef.getValue(bt)))
    table.append(row);
  });
  $('.tables').append(table);
}

function displayTile(tile) {
  let active = false;
  if (tile >= COMPONENTS.length - 3) {
    tile -= COMPONENTS.length - 3;
    if (tile < COMPONENTS.length - 3)
      active = true;
  }
  const result = $('<span>' + COMPONENTS[tile].name + '</span>').addClass(COMPONENTS[tile].className);
  if (active) {
    result.attr('title', `Active ${COMPONENTS[tile].title}`);
    result.css('outline', '2px dashed black')
  } else {
    result.attr('title', COMPONENTS[tile].title);
  }
  return result;
}

function saveTile(tile) {
  if (tile >= COMPONENTS.length) {
    tile -= COMPONENTS.length - 3;
    if (tile < COMPONENTS.length - 3) {
      return COMPONENTS[tile].saveName.replaceAll("nuclearcraft:", "nuclearcraft:active_");
    }
  }
  return COMPONENTS[tile].saveName
}

function formatInfo(label, value, unit) {
  const row = $('<div></div>').addClass('info');
  row.append('<div>' + label + '</div>');
  row.append('<div>' + unit + '</div>');
  row.append(Math.round(value * 100) / 100);
  return row
}

$(() => { FissionOpt().then((FissionOpt) => {
  createBlockTypeTable()
  const run = $('#run'), pause = $('#pause'), stop = $('#stop'), design = $('#design');
  const save = $('#save'), bgString = $('#bgString'), progress = $('#progress');
  const fuelBasePower = $('#fuelBasePower'), fuelBaseHeat = $('#fuelBaseHeat');
  const settings = new FissionOpt.FissionSettings();
  let lossElement, lossPlot, opt = null, timeout = null;

  const fuelPresets = {
    TBU: [60*80, 18],
    LEU235: [120*80, 50],
    HEU235: [480*80, 300],
    LEU233: [144*80, 60],
    HEU233: [576*80, 360],
    LEN236: [90*80, 36],
    HEN236: [360*80, 216],
    LEP239: [105*80, 40],
    HEP239: [420*80, 240],
    LEP241: [165*80, 70],
    HEP241: [660*80, 420],
    LEA242: [192*80, 94],
    HEA242: [768*80, 564],
    LECm243: [210*80, 112],
    HECm243: [840*80, 672],
    LECm245: [162*80, 68],
    HECm245: [648*80, 408],
    LECm247: [138*80, 54],
    HECm247: [552*80, 324],
    LEB248: [135*80, 52],
    HEB248: [540*80, 312],
    LECf249: [216*80, 116],
    HECf249: [864*80, 696],
    LECf251: [225*80, 120],
    HECf251: [900*80, 720],
  };
  for (const [name, [power, heat]] of Object.entries(fuelPresets)) {
    $('#' + name).click(() => {
      if (opt !== null)
        return;
      fuelBasePower.val(power);
      fuelBaseHeat.val(heat);
    });
  }

  const rates = []
  const limits = []
  const activeRates = []
  const activeLimits = []
  $('#rate input').each(function() { rates.push($(this)); });
  $('#limit input').each(function() { limits.push($(this)); });
  $('#activeRate input').each(function() { activeRates.push($(this)); });
  $('#activeLimit input').each(function() { activeLimits.push($(this)); });


  function updateDisables() {
    $('#settings input').prop('disabled', opt !== null);
    $('#settings a')[opt === null ? 'removeClass' : 'addClass']('disabledLink');
    run[timeout === null ? 'removeClass' : 'addClass']('disabledLink');
    pause[timeout !== null ? 'removeClass' : 'addClass']('disabledLink');
    stop[opt !== null ? 'removeClass' : 'addClass']('disabledLink');
  }

  function displaySample(sample) {
    design.empty();
    let block = $('<div></div>');
    block.append(formatInfo('Max Power', sample.getPower(), 'FE/t'));
    block.append(formatInfo('Avg Power', sample.getAvgPower(), 'FE/t'));
    block.append(formatInfo('Max Power (GT)', sample.getPower() / 8192, 'A (EV)'));
    block.append(formatInfo('Avg Power (GT)', sample.getAvgPower() / 8192, 'A (EV)'));
    block.append(formatInfo('Heat', sample.getHeat(), 'H/t'));
    block.append(formatInfo('Cooling', sample.getCooling(), 'H/t'));
    block.append(formatInfo('Net Heat', sample.getNetHeat(), 'H/t'));
    block.append(formatInfo('Duty Cycle', sample.getDutyCycle() * 100, '%'));
    block.append(formatInfo('Fuel Use Rate', sample.getAvgBreed(), '&times;'));
    block.append(formatInfo('Efficiency', sample.getEfficiency() * 100, '%'));
    design.append(block);

    const shapes = [], strides = [], data = sample.getData();
    for (let i = 0; i < 3; ++i) {
      shapes.push(sample.getShape(i));
      strides.push(sample.getStride(i));
    }
    let resourceMap = {};
    resourceMap[-1] = (shapes[0] * shapes[1] + shapes[1] * shapes[2] + shapes[2] * shapes[0]) * 2 + (shapes[0] + shapes[1] + shapes[2]) * 4 + 8;
    for (let y = 0; y < shapes[0]; ++y) {
      block = $('<div></div>');
      block.append('<div>Layer ' + (y + 1) + '</div>');
      for (let z = 0; z < shapes[1]; ++z) {
        const row = $('<div></div>').addClass('row');
        for (let x = 0; x < shapes[2]; ++x) {
          if (x)
            row.append(' ');
          const tile = data[y * strides[0] + z * strides[1] + x * strides[2]];
          if (!resourceMap.hasOwnProperty(tile))
            resourceMap[tile] = 1;
          else
            ++resourceMap[tile];
          row.append(displayTile(tile));
        }
        block.append(row);
      }
      design.append(block);
    }

    save.removeClass('disabledLink');
    save.off('click').click(async () => {
      let internalMap = {}
      let palette = {};
      let data = sample.getData();
      let internalIndex = 0;
      let blockData = new Int8Array(shapes[0] * shapes[1] * shapes[2]);
      for (let y = 0; y < shapes[0]; ++y) {
        for (let z = 0; z < shapes[1]; ++z) {
          for (let x = 0; x < shapes[2]; ++x) {
            const index = y * strides[0] + z * strides[1] + x * strides[2];
            const savedTile = saveTile(data[index]);
            if (!internalMap.hasOwnProperty(savedTile)) {
              palette[savedTile] = new Int32(internalIndex);
              blockData[index] = internalIndex;
              internalMap[savedTile] = internalIndex++;
            } else {
              blockData[index] = internalMap[savedTile];
            }
          }
        }
      }
      let res = await write({
        Width: new Int16(shapes[2]),
        Height: new Int16(shapes[0]),
        Length: new Int16(shapes[1]),
        Version: new Int32(2),
        DataVersion: new Int32(3465),
        PaletteMax: new Int32(Object.keys(palette).length),
        Palette: palette,
        BlockData: blockData
      });
      const elem = document.createElement('a');
      const url = window.URL.createObjectURL(new Blob([res]));
      elem.setAttribute('href', url);
      elem.setAttribute('download', 'reactor.schem');
      elem.click();
      window.URL.revokeObjectURL(url);
    });

    bgString.removeClass('disabledLink');
    bgString.off('click').click(async () => {
      let internalMap = {};
      let data = sample.getData();
      let internalIndex = 0;
      let stateList = [];
      for (let y = 0; y < shapes[1]; ++y) {
        for (let z = 0; z < shapes[0]; ++z) {
          for (let x = 0; x < shapes[2]; ++x) {
            const savedTile = `{Name:\\"${saveTile(data[z * shapes[2] * shapes[1] + y * shapes[2] + x])}\\"}`;
            if (!internalMap.hasOwnProperty(savedTile)) {
              stateList.push(internalIndex);
              internalMap[savedTile] = internalIndex++;
            } else {
              stateList.push(internalMap[savedTile]);
            }
          }
        }
      }
      let string = `{"statePosArrayList": "{blockstatemap:[${Object.keys(internalMap)}],startpos:{X:0,Y:0,Z:0},` +
          `endpos:{X:${shapes[2] - 1},Y:${shapes[0] - 1},Z:${shapes[1] - 1}},statelist:[I;${stateList}]}"}`;
      await navigator.clipboard.writeText(string);
    });

    block = $('<div></div>');
    block.append('<div>Total number of blocks used</div>')
    resourceMap = Object.entries(resourceMap);
    resourceMap.sort((x, y) => y[1] - x[1]);
    for (let resource of resourceMap) {
      if (parseInt(resource[0]) === (COMPONENTS.length - 3) * 2 + 2) continue;
      const row = $('<div></div>');
      if (resource[0] < 0)
        row.append('Casing');
      else
        row.append(displayTile(resource[0]).addClass('row'));
      block.append(row.append(' &times; ' + resource[1]));
    }
    design.append(block);
  }

  function step() {
    timeout = window.setTimeout(step, 0);
    opt.stepInteractive();
    const nStage = opt.getNStage();
    if (nStage === -2) {
      progress.text('Episode ' + opt.getNEpisode() + ', training iteration ' + opt.getNIteration());
    } else if (nStage === -1) {
      progress.text('Episode ' + opt.getNEpisode() + ', inference iteration ' + opt.getNIteration());
    } else {
      progress.text('Episode ' + opt.getNEpisode() + ', stage ' + nStage + ', iteration ' + opt.getNIteration());
    }

    if (opt.needsRedrawBest()) {
      displaySample(opt.getBest());
    }
    if (opt.needsReplotLoss()) {
      const data = opt.getLossHistory();
      while (lossPlot.data.labels.length < data.length)
        lossPlot.data.labels.push(lossPlot.data.labels.length);
      lossPlot.data.datasets[0].data = data;
      lossPlot.update({duration: 0});
    }
  }

  run.click(() => {
    if (timeout !== null)
      return;
    if (opt === null) {
      const parseSize = (x) => {
        const result = parseInt(x);
        if (!(result > 0))
          throw Error("Core size must be a positive integer");
        return result;
      };
      const parsePositiveFloat = (name, x) => {
        const result = parseFloat(x);
        if (!(result >= 0))
          throw Error(name + " must be a positive number");
        return result;
      };
      const parseLimits = (x) => {
        let result = parseInt(x)
        result = result >= 0 && !isNaN(result) ? result : -1;
        return result;
      };
      try {
        settings.sizeX = parseSize($('#sizeX').val());
        settings.sizeY = parseSize($('#sizeY').val());
        settings.sizeZ = parseSize($('#sizeZ').val());
        settings.fuelBasePower = parsePositiveFloat('Fuel Base Power', fuelBasePower.val());
        settings.fuelBaseHeat = parsePositiveFloat('Fuel Base Heat', fuelBaseHeat.val());
        settings.ensureHeatNeutral = $('#ensureHeatNeutral').is(':checked');
        settings.goal = parseInt($('input[name=goal]:checked').val());
        settings.symX = $('#symX').is(':checked');
        settings.symY = $('#symY').is(':checked');
        settings.symZ = $('#symZ').is(':checked');
        settings.genMult = parsePositiveFloat('Generation Multiplier', $('#genMult').val());
        settings.heatMult = parsePositiveFloat('Heat Multiplier', $('#heatMult').val());
        settings.modFEMult = parsePositiveFloat('Moderator FE Multiplier', $('#modFEMult').val());
        settings.modHeatMult = parsePositiveFloat('Moderator Heat Multiplier', $('#modHeatMult').val());
        settings.FEGenMult = parsePositiveFloat('FE Generation Multiplier', $('#FEGenMult').val());
        settings.activeComponentPrime = $('#activeComponentPrime').is(':checked');

        let index = 0
        for (let i = 0; i < COMPONENTS.length - 3; i++){
          settings.setLimit(index, parseLimits(limits[i].val()))
          settings.setRate(index, parsePositiveFloat('Cooling Rate', rates[i].val()));
          index++;
        }
        for (let i = 0; i < COMPONENTS.length; i++) {
          let value = COMPONENTS[i];
          if (value.active_cooling_rate) {
            settings.setLimit(index, parseLimits(activeLimits[i].val()))
            settings.setRate(index, parsePositiveFloat('Cooling Rate', activeRates[i].val()));
          }
          index++;
        }
        settings.setLimit((COMPONENTS.length - 3) * 2, parseLimits(limits[COMPONENTS.length - 3].val())) // Cells
        settings.setLimit((COMPONENTS.length - 3) * 2 + 1 , parseLimits(limits[COMPONENTS.length - 2].val())) // Moderators
      } catch (error) {
        alert('Error: ' + error.message);
        return;
      }
      design.empty();
      save.off('click');
      save.addClass('disabledLink');
      if (lossElement !== undefined) lossElement.remove();
      const useNet = $('#useNet').is(':checked');
      if (useNet) {
        lossElement = $('<canvas></canvas>').attr('width', 1024).attr('height', 128).insertAfter(progress);
        lossPlot = new Chart(lossElement[0].getContext('2d'), {
          type: 'bar',
          options: {responsive: false, animation: {duration: 0}, hover: {animationDuration: 0}, scales: {xAxes: [{display: false}]}, legend: {display: false}},
          data: {labels: [], datasets: [{label: 'Loss', backgroundColor: 'red', data: [], categoryPercentage: 1.0, barPercentage: 1.0}]}
        });
      }
      opt = new FissionOpt.FissionOpt(settings, useNet);
    }
    timeout = window.setTimeout(step, 0);
    updateDisables();
  });
  pause.click(() => {
    if (timeout === null)
      return;
    window.clearTimeout(timeout);
    timeout = null;
    updateDisables();
  });
  stop.click(() => {
    if (opt === null)
      return;
    if (timeout !== null) {
      window.clearTimeout(timeout);
      timeout = null;
    }
    opt.delete();
    opt = null;
    updateDisables();
  })
})});
