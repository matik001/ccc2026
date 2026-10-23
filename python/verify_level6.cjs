// Weryfikacja wynikow oryginalna logika symulatora z archiwum konkursowego.
// Uruchomienie z dowolnego katalogu: node python/verify_level6.cjs
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const root = path.resolve(__dirname, '..');
const sourceDir = path.join(root, 'level-6');
const outputDir = path.resolve(process.argv[2] || path.join(__dirname, 'outputs'));
const html = fs.readFileSync(path.join(sourceDir, 'sandbox-level-6.html'), 'utf8');

function extract(start, end) {
    const first = html.indexOf(start);
    const last = html.indexOf(end, first);
    assert(first >= 0 && last > first, 'Nie znaleziono kodu symulatora.');
    return html.slice(first, last);
}

// Uruchamiamy oryginalny parser i silnik, bez interfejsu przegladarki.
const core = extract('    const DIR_TO = {', '    const TRACK_COLORS');
const parser = extract('    function parseHexInput(', '    function serializeInput(');
const run = new Function('gameState', 'input', 'program', `
    const INSTR_FORWARD = 'F';
    const INSTR_BACKWARD = 'B';
    const INSTR_TOGGLE = 'T';
    ${core}
    ${parser}
    parseHexInput(gameState, input);
    if (gameState.inputParseErrors.length) {
        throw new Error(gameState.inputParseErrors.join('; '));
    }
    gameState.program = program;
    for (let step = 0; step < program.length; step++) applyEmitterStep(step);
    return gameState.acceptedPatterns;
`);

let totalCases = 0;
let totalInstructions = 0;
for (const name of fs.readdirSync(sourceDir).filter(n => /^in_level-6_.*\.txt$/.test(n)).sort()) {
    const lines = fs.readFileSync(path.join(sourceDir, name), 'utf8').trim().split(/\r?\n/)
        .map(line => line.trim()).filter(Boolean);
    const count = Number(lines[0]);
    const outputName = name.replace(/^in_/, 'out_');
    const output = fs.readFileSync(path.join(outputDir, outputName), 'utf8').trim();
    const blocks = output.split(/\r?\n\s*\r?\n/);
    assert.equal(blocks.length, count, `${name}: liczba programow`);
    let offset = 1;
    let score = 0;
    for (let index = 0; index < count; index++) {
        const header = lines[offset].split(/\s+/).map(Number);
        const [, rows, , required] = header;
        const input = lines.slice(offset, offset + rows + 1).join('\n');
        offset += rows + 1;
        const program = blocks[index].split(/\r?\n/).map(line => line.trim().split(/\s+/));
        assert(program.every(tick => tick.length === 6 && tick.every(op => /^[FBT-]$/.test(op))),
            `${name}, wzor ${index + 1}: format instrukcji`);
        const state = {
            radius: 0, cols: 0, rows: 0,
            pattern: {grid: [], rows: 0, cols: 0, count: 0, patternRadius: 0},
            acceptedPatterns: 0,
            emitter: {perimeter: [], indices: [], active: [], baseGrid: [], grid: [],
                possibleEmitterStates: [], rayCells: []},
            program: [], precomputedStates: [], inputParseErrors: [], solutionParseErrors: [],
        };
        const actual = run(state, input, program);
        assert.equal(actual, required, `${name}, wzor ${index + 1}: liczba odebranych wzorow`);
        score += program.length;
    }
    assert.equal(offset, lines.length, `${name}: nadmiarowe dane`);
    totalCases += count;
    totalInstructions += score;
    console.log(`PASS: ${outputName}: ${count} programow, ${score} instrukcji.`);
}
console.log(`PASS: ${totalCases} programow; laczny wynik ${totalInstructions} instrukcji.`);
