package tracedoctor

import (
	"fmt"
	"github.com/vespa-engine/vespa/client/go/internal/vespa/slime"
	"io"
)

type threadSummary struct {
	matchMs       float64
	firstPhaseMs  float64
	secondPhaseMs float64
}

func (p *threadSummary) render(out *output) {
	out.fmt("+---------------+---------------+\n")
	out.fmt("| Matching      | %10.3f ms |\n", p.matchMs)
	out.fmt("| First phase   | %10.3f ms |\n", p.firstPhaseMs)
	out.fmt("| Second phase  | %10.3f ms |\n", p.secondPhaseMs)
	out.fmt("+---------------+---------------+\n")
}

type timing struct {
	queryMs   float64
	summaryMs float64
	totalMs   float64
}

func (t *timing) render(out *output) {
	if t == nil {
		return
	}
	out.fmt("+---------+---------------+\n")
	out.fmt("| Total   | %10.3f ms |\n", t.totalMs)
	out.fmt("+---------+---------------+\n")
	out.fmt("| Query   | %10.3f ms |\n", t.queryMs)
	out.fmt("| Summary | %10.3f ms |\n", t.summaryMs)
	out.fmt("| Other   | %10.3f ms |\n", t.totalMs-t.queryMs-t.summaryMs)
	out.fmt("+---------+---------------+\n")
}

func extractTiming(queryResult slime.Value) *timing {
	obj := queryResult.Field("timing")
	if !obj.Valid() {
		return nil
	}
	return &timing{
		queryMs:   obj.Field("querytime").AsDouble() * 1000.0,
		summaryMs: obj.Field("summaryfetchtime").AsDouble() * 1000.0,
		totalMs:   obj.Field("searchtime").AsDouble() * 1000.0,
	}
}

type output struct {
	out io.Writer
	err error
}

func (out *output) fmt(format string, args ...interface{}) {
	if out.err == nil {
		_, out.err = fmt.Fprintf(out.out, format, args...)
	}
}

type Context struct {
	root   slime.Value
	timing *timing
}

func NewContext(root slime.Value) *Context {
	return &Context{
		root:   root,
		timing: extractTiming(root),
	}
}

func selectSlowestSearch(traces []protonTrace) (int, *protonTrace, float64) {
	var slowest *protonTrace
	var slowestDuration float64
	var totalDuration float64
	for i := range traces {
		duration := traces[i].durationMs()
		totalDuration += duration
		if slowest == nil || duration > slowestDuration {
			slowest = &traces[i]
			slowestDuration = duration
		}
	}
	others := totalDuration - slowestDuration
	if len(traces) > 1 {
		others /= float64(len(traces) - 1)
	}
	return len(traces), slowest, others
}

func selectSlowestThread(threads []threadTrace) (int, *threadTrace, float64) {
	var slowest *threadTrace
	var slowestPerf float64
	var totalPerf float64
	for i := range threads {
		perf := threads[i].profTimeMs()
		totalPerf += perf
		if slowest == nil || perf > slowestPerf {
			slowest = &threads[i]
			slowestPerf = perf
		}
	}
	others := totalPerf - slowestPerf
	if len(threads) > 1 {
		others /= float64(len(threads) - 1)
	}
	return len(threads), slowest, others
}

func (ctx *Context) analyzeThread(trace protonTrace, thread threadTrace, out *output) {
	thread.timeline().render(out)
	threadSummary := threadSummary{
		matchMs:       thread.matchTimeMs(),
		firstPhaseMs:  thread.firstPhaseTimeMs(),
		secondPhaseMs: thread.secondPhaseTimeMs(),
	}
	threadSummary.render(out)
	queryPerf := trace.extractQuery()
	queryPerf.importMatchPerf(thread)
	out.fmt("\nMatch profiling for thread #%d (total time was %f ms):\n", thread.id, thread.matchTimeMs())
	queryPerf.render(out)
	out.fmt("\nFirst phase rank profiling for thread #%d (total time was %f ms):\n", thread.id, thread.firstPhaseTimeMs())
	thread.firstPhasePerf().render(out)
	out.fmt("\nSecond phase rank profiling for thread #%d (total time was %f ms):\n", thread.id, thread.secondPhaseTimeMs())
	thread.secondPhasePerf().render(out)
}

func (ctx *Context) analyzeProtonTrace(trace protonTrace, out *output) {
	trace.timeline().render(out)
	cnt, worst, peers := selectSlowestThread(trace.findThreadTraces())
	if worst != nil {
		out.fmt("found %d thread%s, slowest matching/ranking was thread #%d: %.3f ms\n",
			cnt, suffix(cnt, "s"), worst.id, worst.profTimeMs())
		if cnt > 1 {
			out.fmt("(average of other threads was %.3f ms)\n", peers)
		}
		ctx.analyzeThread(trace, *worst, out)
		if ann := newAnnProbe(trace); ann.impact() != 0.0 {
			ann.render(out)
		}
	}
}

func selectSlowestGroup(groups []protonTraceGroup) int {
	var slowestIndex int
	var maxDuration float64
	for i, group := range groups {
		duration := group.durationMs()
		if duration > maxDuration {
			maxDuration = duration
			slowestIndex = i
		}
	}
	return slowestIndex
}

type searchMeta struct {
	groups []protonTraceGroup
}

func (s searchMeta) render(out *output) {
	out.fmt("+--------+-------+---------------+\n")
	out.fmt("| search | nodes | back-end time |\n")
	out.fmt("+--------+-------+---------------+\n")
	for _, group := range s.groups {
		groupID := group.id
		nodes := len(group.traces)
		docType := group.documentType()
		duration := group.durationMs()
		out.fmt("| %6d | %5d | %10.3f ms | %s\n", groupID, nodes, duration, docType)
	}
	out.fmt("+--------+-------+---------------+\n")
}

func (ctx *Context) Analyze(stdout io.Writer) error {
	out := &output{out: stdout}
	ctx.timing.render(out)
	groups := groupProtonTraces(findProtonTraces(ctx.root))
	if len(groups) > 0 {
		out.fmt("found %d search%s:\n", len(groups), suffix(len(groups), "es"))
		searchMeta{groups}.render(out)
		idx := selectSlowestGroup(groups)
		cnt, worst, peers := selectSlowestSearch(groups[idx].traces)
		if worst != nil {
			out.fmt("slowest content node was: %s[%d]: %.3f ms\n",
				worst.documentType(), worst.distributionKey(), worst.durationMs())
			if cnt > 1 {
				out.fmt("(average of other content nodes for the same search was %.3f ms)\n", peers)
			}
			ctx.analyzeProtonTrace(*worst, out)
		}
	}
	return out.err
}
