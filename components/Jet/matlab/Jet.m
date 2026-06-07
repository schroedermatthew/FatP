% FATP_META:
%   meta_version: 1
%   component: Jet
%   file_role: port_class
%   path: Jet/matlab/Jet.m
%   language: matlab
%   layer: Foundation
%   summary: Forward-mode AD scalar; MATLAB port of the C++ Jet<N>.
%   api_stability: in_work
%   related:
%     cpp_source: include/fat_p/Jet.h
%     docs_search: "Jet"
%     tests_search: "test_Jet"
%   hygiene:
%     toolboxes: none
classdef Jet
    % JET  Fixed-size forward-mode automatic differentiation scalar.
    %
    %   A Jet carries a scalar value together with its partial derivatives with
    %   respect to N independent variables, propagated through arithmetic and
    %   the elementary functions by the chain rule. A function written over its
    %   scalar type, evaluated on seeded Jets, returns both its value and its
    %   gradient (or, stacked, a Jacobian) at the seed point.
    %
    %   This is a MATLAB port of the Fat-P C++ Jet<N>; it uses the same
    %   derivative formulas and the same overflow/domain guards. Differences
    %   from the C++ version are noted in the methods (1-based seeding; the
    %   partial count N is a run-time property, not a template parameter; out-of
    %   -domain sqrt/log return real NaN rather than MATLAB's complex result; for
    %   a non-positive base with a Jet exponent the real value is preserved where
    %   defined, but the partials are NaN under the positive-base derivative
    %   contract).
    %
    %   Construction:
    %     Jet(value, partials)      value scalar, partials a 1-by-N row vector
    %     Jet.constant(value, N)    a constant (zero partials)
    %     Jet.seed(value, k, N)     a variable: unit derivative in direction k
    %     Jet.vars([x1 ... xN])     a 1-by-N Jet array, each seeded in its slot
    %
    %   Readout:
    %     y.v / value(y)            the value
    %     y.d / grad(y)             the 1-by-N gradient
    %     [f, J] = Jet.jacobian(Y)  values and Jacobian of a Jet array
    %
    %   Example:
    %     X  = Jet.vars([0.7, 1.2]);
    %     y  = sin(X(1)) .* X(2) + exp(X(2));
    %     g  = grad(y);             % [dy/dx1, dy/dx2]
    %
    %   See also: the C++ Jet (OV-JET-001, UM-JET-001, CG-JET-001).

    properties
        v (1,1) double = 0           % the value
        d (1,:) double = zeros(1,0)  % the partial derivatives, 1-by-N
    end

    methods
        function obj = Jet(value, partials)
            % JET  Construct from a scalar value and a 1-by-N partial vector.
            if nargin == 0
                return % default: v = 0, d = 1-by-0 (from the property defaults)
            end
            if nargin == 1
                error('Jet:ctor', ...
                    'use Jet(value, partials) or Jet.seed / Jet.constant / Jet.vars');
            end
            obj.v = value;
            obj.d = partials;
        end

        % ---- readout -------------------------------------------------------
        function val = value(obj)
            % VALUE  Scalar value, or an array of values for a Jet array.
            if isscalar(obj)
                val = obj.v;
            else
                val = reshape([obj.v], size(obj));
            end
        end

        function g = grad(obj)
            % GRAD  The 1-by-N gradient of a scalar Jet.
            if ~isscalar(obj)
                error('Jet:notScalar', ...
                    'grad applies to a scalar Jet; use Jet.jacobian for arrays');
            end
            g = obj.d;
        end

        % ---- arithmetic ----------------------------------------------------
        function r = plus(a, b)
            [av, ad, bv, bd] = Jet.binargs(a, b);
            r = Jet(av + bv, ad + bd);
        end

        function r = minus(a, b)
            [av, ad, bv, bd] = Jet.binargs(a, b);
            r = Jet(av - bv, ad - bd);
        end

        function r = uminus(a)
            r = Jet(-a.v, -a.d);
        end

        function r = uplus(a)
            r = a;
        end

        function r = times(a, b)
            if isa(a, 'Jet') && ~isa(b, 'Jet')       % Jet .* scalar: mirrors C++ Jet*double
                bv = double(b);
                r = Jet(a.v * bv, a.d .* bv);
                return
            elseif isa(b, 'Jet') && ~isa(a, 'Jet')   % scalar .* Jet
                av = double(a);
                r = Jet(av * b.v, av .* b.d);
                return
            end
            [av, ad, bv, bd] = Jet.binargs(a, b);
            r = Jet(av * bv, av * bd + bv * ad);
        end

        function r = mtimes(a, b)
            r = times(a, b); % scalar value type: * and .* coincide
        end

        function r = rdivide(a, b)
            if isa(a, 'Jet') && ~isa(b, 'Jet')   % Jet / scalar: dedicated overload, mirrors C++
                bv = double(b);
                r = Jet(a.v / bv, a.d ./ bv);
                return
            end
            [av, ad, bv, bd] = Jet.binargs(a, b);
            value = av / bv;
            r = Jet(value, (ad - value * bd) / bv);
        end

        function r = mrdivide(a, b)
            r = rdivide(a, b);
        end

        function r = power(a, b)
            if isa(a, 'Jet') && isa(b, 'Jet')
                r = Jet.powJJ(a, b);
            elseif isa(a, 'Jet')
                r = Jet.powJS(a, double(b));
            else
                r = Jet.powSJ(double(a), b);
            end
        end

        function r = mpower(a, b)
            r = power(a, b);
        end

        % ---- comparisons (value only, matching the C++ ordering) -----------
        function t = lt(a, b)
            [av, ~, bv, ~] = Jet.binargs(a, b);
            t = av < bv;
        end

        function t = le(a, b)
            [av, ~, bv, ~] = Jet.binargs(a, b);
            t = av <= bv;
        end

        function t = gt(a, b)
            [av, ~, bv, ~] = Jet.binargs(a, b);
            t = av > bv;
        end

        function t = ge(a, b)
            [av, ~, bv, ~] = Jet.binargs(a, b);
            t = av >= bv;
        end

        function t = eq(a, b)
            [av, ~, bv, ~] = Jet.binargs(a, b);
            t = av == bv;
        end

        function t = ne(a, b)
            [av, ~, bv, ~] = Jet.binargs(a, b);
            t = av ~= bv;
        end

        % ---- elementary functions ------------------------------------------
        function r = sin(x)
            r = Jet(sin(x.v), cos(x.v) .* x.d);
        end

        function r = cos(x)
            r = Jet(cos(x.v), -sin(x.v) .* x.d);
        end

        function r = tan(x)
            c = cos(x.v);
            r = Jet(tan(x.v), x.d ./ (c * c));
        end

        function r = exp(x)
            e = exp(x.v);
            r = Jet(e, e .* x.d);
        end

        function r = log(x)
            % Real domain: negative input gives NaN (not MATLAB's complex log).
            if x.v < 0
                r = Jet(NaN, NaN(1, numel(x.d)));
            else
                r = Jet(log(x.v), x.d ./ x.v); % v == 0: log -> -Inf, partials Inf/NaN
            end
        end

        function r = sqrt(x)
            % Real domain: negative input gives NaN (not MATLAB's complex sqrt).
            if x.v < 0
                r = Jet(NaN, NaN(1, numel(x.d)));
            else
                sv = sqrt(x.v);
                if sv > 0
                    coeff = 0.5 / sv;
                else
                    coeff = 0; % derivative 0 at the boundary, by convention (matches C++)
                end
                r = Jet(sv, coeff .* x.d);
            end
        end

        function r = asin(x)
            vv = x.v;
            if abs(vv) > 1
                r = Jet(NaN, NaN(1, numel(x.d)));
            else
                r = Jet(asin(vv), x.d ./ sqrt((1 - vv) * (1 + vv)));
            end
        end

        function r = acos(x)
            vv = x.v;
            if abs(vv) > 1
                r = Jet(NaN, NaN(1, numel(x.d)));
            else
                r = Jet(acos(vv), -x.d ./ sqrt((1 - vv) * (1 + vv)));
            end
        end

        function r = atan(x)
            % Large-argument guard: for |v| > 1 compute via 1/v so v*v cannot
            % overflow and round the true small derivative down to zero.
            vv = x.v;
            if abs(vv) > 1
                iv = 1 / vv;
                deriv = (iv * iv) / (iv * iv + 1);
            else
                deriv = 1 / (1 + vv * vv);
            end
            r = Jet(atan(vv), deriv .* x.d);
        end

        function r = abs(x)
            r = Jet(abs(x.v), sign(x.v) .* x.d);
        end

        function r = hypot(a, b)
            [av, ad, bv, bd] = Jet.binargs(a, b);
            if isnan(av) || isnan(bv)
                r = Jet(NaN, NaN(1, numel(ad)));
                return
            end
            h = hypot(av, bv); % built-in hypot is overflow-safe for the value
            % Coefficients through a scaled norm so they stay finite when h
            % overflows for a large finite pair (matches C++).
            scale = max(abs(av), abs(bv));
            if scale == 0
                r = Jet(0, zeros(1, numel(ad)));
            else
                xs = av / scale; ys = bv / scale;
                hs = sqrt(xs * xs + ys * ys);
                hx = xs / hs; hy = ys / hs;
                r = Jet(h, hx .* ad + hy .* bd);
            end
        end

        function r = atan2(a, b)
            % ATAN2(Y, X) for Jets. Scaled denominator so x*x + y*y cannot
            % overflow at large magnitude (matching the C++ guard).
            [yv, yd, xv, xd] = Jet.binargs(a, b);
            if isnan(yv) || isnan(xv)
                r = Jet(NaN, NaN(1, numel(yd)));
                return
            end
            s = max(abs(xv), abs(yv));
            val = atan2(yv, xv);
            if s == 0
                r = Jet(val, zeros(1, numel(yd)));
            else
                xs = xv / s;
                ys = yv / s;
                den = xs * xs + ys * ys; % in [1, 2]
                dy = (xs / den) / s;     % d/dy
                dx = -(ys / den) / s;    % d/dx
                r = Jet(val, dy .* yd + dx .* xd);
            end
        end

        % ---- display -------------------------------------------------------
        function disp(obj)
            if isscalar(obj)
                fprintf('  Jet: value = %.10g\n', obj.v);
                if isempty(obj.d)
                    fprintf('       partials = []\n');
                else
                    fprintf('       partials = [ %s]\n', sprintf('%.6g ', obj.d));
                end
            else
                fprintf('  %s Jet array\n', mat2str(size(obj)));
            end
        end
    end

    methods (Static)
        function r = constant(value, N)
            % CONSTANT  A Jet with the given value and zero partials.
            r = Jet(value, zeros(1, N));
        end

        function r = seed(value, k, N)
            % SEED  A variable: value with a unit derivative in direction k
            % (1-based; the C++ version is 0-based).
            if k < 1 || k > N
                error('Jet:badDirection', 'direction k=%d out of range 1..%d', k, N);
            end
            d = zeros(1, N);
            d(k) = 1;
            r = Jet(value, d);
        end

        function V = vars(values)
            % VARS  A 1-by-N Jet array from N input values, each seeded in its
            % own direction. Convenient for setting up a gradient computation.
            values = values(:).';
            N = numel(values);
            V = Jet.empty(1, 0);
            for i = 1:N
                V(i) = Jet.seed(values(i), i, N);
            end
        end

        function [f, J] = jacobian(Y)
            % JACOBIAN  Values f (M-by-1) and Jacobian J (M-by-N) of a Jet array.
            M = numel(Y);
            if M == 0
                f = zeros(0, 1);
                J = zeros(0, 0);
                return
            end
            N = numel(Y(1).d);
            f = zeros(M, 1);
            J = zeros(M, N);
            for i = 1:M
                if numel(Y(i).d) ~= N
                    error('Jet:dimMismatch', 'inconsistent partial counts across outputs');
                end
                f(i) = Y(i).v;
                J(i, :) = Y(i).d;
            end
        end
    end

    methods (Static, Access = private)
        function [av, ad, bv, bd] = binargs(a, b)
            % Promote a scalar operand to a constant Jet and return the value /
            % partial pairs. Mixing Jets of different N, or a Jet with a non-
            % scalar, is an error.
            if isa(a, 'Jet') && isa(b, 'Jet')
                N = numel(a.d);
                if numel(b.d) ~= N
                    error('Jet:dimMismatch', ...
                        'partial-count mismatch: %d vs %d', N, numel(b.d));
                end
                av = a.v; ad = a.d; bv = b.v; bd = b.d;
            elseif isa(a, 'Jet')
                if ~isscalar(b)
                    error('Jet:nonScalar', 'a Jet may only combine with a scalar');
                end
                av = a.v; ad = a.d; bv = double(b); bd = zeros(1, numel(a.d));
            else
                if ~isscalar(a)
                    error('Jet:nonScalar', 'a Jet may only combine with a scalar');
                end
                av = double(a); ad = zeros(1, numel(b.d)); bv = b.v; bd = b.d;
            end
        end

        function r = powJS(x, p)
            % x .^ p, p a constant scalar.
            N = numel(x.d);
            if p == 0
                r = Jet(1, zeros(1, N)); % x^0 is constant 1, zero partials
                return
            end
            vv = x.v;
            if vv < 0 && floor(p) ~= p
                r = Jet(NaN, NaN(1, N)); % negative base, non-integer power
                return
            end
            % Finding B: for the root-like family 0 < p < 1 at vv == 0 the true
            % slope is +Inf; return 0 by the non-poisoning convention sqrt uses,
            % so x.^0.5 and sqrt(x) agree everywhere.
            if vv == 0 && p > 0 && p < 1
                coeff = 0;
            else
                coeff = p * vv ^ (p - 1);
            end
            r = Jet(vv ^ p, coeff .* x.d);
        end

        function r = powSJ(s, y)
            % s .^ y, s a constant scalar. For a non-positive base the value is
            % kept where the real power is defined, but the gradient is NaN --
            % the log-based derivative is not real. Mirrors C++ pow(double, Jet).
            N = numel(y.d);
            if ~(s > 0)
                val = s ^ y.v;
                if ~isreal(val)
                    val = NaN; % negative base, non-integer exponent -> off the real line
                end
                r = Jet(val, NaN(1, N));
                return
            end
            val = s ^ y.v;
            r = Jet(val, (val * log(s)) .* y.d);
        end

        function r = powJJ(x, y)
            % x .^ y, both Jets. For a non-positive base the value is kept where
            % the real power is defined, but the gradient is NaN under the
            % positive-base derivative contract. Mirrors C++ pow(Jet, Jet).
            N = numel(x.d);
            if ~(x.v > 0)
                val = x.v ^ y.v;
                if ~isreal(val)
                    val = NaN;
                end
                r = Jet(val, NaN(1, N));
                return
            end
            val = x.v ^ y.v;
            dx = y.v * x.v ^ (y.v - 1); % d/dx
            dy = val * log(x.v);        % d/dy
            r = Jet(val, dx .* x.d + dy .* y.d);
        end
    end
end
